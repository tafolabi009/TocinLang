// Tocin runtime: goroutines (OS threads), channels, and exception handling.
//
// These symbols are linked into the compiler/runtime and resolved by the JIT
// from the running process, and linked into native executables via the normal
// C toolchain. Values flowing through channels are passed as 64-bit slots
// (ints, bit-cast floats, or pointers), matching the codegen ABI.
#include <atomic>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <csetjmp>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

// POSIX sockets for the TCP networking runtime (Linux / macOS / BSD). On other
// platforms the networking builtins compile to safe error-returning stubs.
#if defined(__unix__) || defined(__APPLE__) || defined(__linux__)
#define TOCIN_HAVE_POSIX_NET 1
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

// ===========================================================================
// Memory: central allocator. When built with -DTOCIN_HAVE_GC and linked
// against the Boehm-Demers-Weiser collector (libgc), every Tocin heap
// allocation is garbage-collected — unreachable arrays, strings, closures,
// and boxes are reclaimed automatically, so long-running programs do not leak.
// Without GC it is a thin malloc wrapper (allocations leak unless freed).
// ===========================================================================
#ifdef TOCIN_HAVE_GC
extern "C"
{
    void GC_init(void);
    void *GC_malloc(size_t);
    void *GC_realloc(void *, size_t);
    void GC_free(void *);
    void GC_allow_register_threads(void);
    int GC_register_my_thread(void *);   // GC_stack_base *
    int GC_unregister_my_thread(void);
    int GC_get_stack_base(void *);        // GC_stack_base *
}
namespace { struct GC_stack_base { void *mem_base; void *reg_base; }; }
#endif

namespace
{
    struct Channel
    {
        std::mutex m;
        std::condition_variable cv;
        std::deque<int64_t> q;
    };

    // Track spawned goroutines so the program can wait for them to finish.
    std::mutex g_threadsMutex;
    std::vector<std::thread> g_threads;

#ifdef TOCIN_HAVE_GC
    std::once_flag g_gcInit;
    void tocin_gc_ensure_init()
    {
        std::call_once(g_gcInit, [] {
            GC_init();
            GC_allow_register_threads();
        });
    }
#endif
}

extern "C"
{
    // Central allocator used by both the compiler-emitted code and the runtime.
    void *__tocin_alloc(int64_t size)
    {
        if (size < 0) size = 0;
#ifdef TOCIN_HAVE_GC
        tocin_gc_ensure_init();
        return GC_malloc((size_t)size);
#else
        return std::malloc((size_t)size);
#endif
    }
    // Resize a buffer obtained from __tocin_alloc.
    void *__tocin_realloc(void *p, int64_t size)
    {
        if (size < 0) size = 0;
#ifdef TOCIN_HAVE_GC
        tocin_gc_ensure_init();
        return GC_realloc(p, (size_t)size);
#else
        return std::realloc(p, (size_t)size);
#endif
    }
}

extern "C"
{
    // Allocate a new channel and return an opaque handle.
    void *__tocin_chan_new()
    {
        return new Channel();
    }

    // Send a 64-bit value into the channel and wake a waiting receiver.
    void __tocin_chan_send(void *handle, int64_t value)
    {
        if (!handle)
            return;
        auto *ch = static_cast<Channel *>(handle);
        {
            std::lock_guard<std::mutex> lock(ch->m);
            ch->q.push_back(value);
        }
        ch->cv.notify_one();
    }

    // Receive a 64-bit value, blocking until one is available.
    int64_t __tocin_chan_recv(void *handle)
    {
        if (!handle)
            return 0;
        auto *ch = static_cast<Channel *>(handle);
        std::unique_lock<std::mutex> lock(ch->m);
        ch->cv.wait(lock, [ch] { return !ch->q.empty(); });
        int64_t value = ch->q.front();
        ch->q.pop_front();
        return value;
    }

    // Non-blocking receive: if a value is available, dequeue it into *out and
    // return 1; otherwise return 0 without blocking. Used to implement `select`.
    int8_t __tocin_chan_try_recv(void *handle, int64_t *out)
    {
        if (!handle || !out)
            return 0;
        auto *ch = static_cast<Channel *>(handle);
        std::lock_guard<std::mutex> lock(ch->m);
        if (ch->q.empty())
            return 0;
        *out = ch->q.front();
        ch->q.pop_front();
        return 1;
    }

    // Briefly sleep to avoid a hot busy-wait in a blocking `select` poll loop.
    void __tocin_chan_park()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    void __tocin_join_all();

    // Spawn a goroutine: run fn(arg) on a new OS thread, tracked for joining.
    void __tocin_go(void (*fn)(void *), void *arg)
    {
        if (!fn)
            return;
        // Ensure all goroutines are joined when the program exits.
        static std::once_flag atexitFlag;
        std::call_once(atexitFlag, [] { std::atexit(__tocin_join_all); });
        std::lock_guard<std::mutex> lock(g_threadsMutex);
#ifdef TOCIN_HAVE_GC
        // Register the goroutine's stack with the collector so values it holds
        // are treated as roots and not reclaimed while in use.
        tocin_gc_ensure_init();
        g_threads.emplace_back([fn, arg] {
            GC_stack_base sb;
            if (GC_get_stack_base(&sb) == 0)
                GC_register_my_thread(&sb);
            fn(arg);
            GC_unregister_my_thread();
        });
#else
        g_threads.emplace_back(fn, arg);
#endif
    }

    // Wait for all spawned goroutines to finish.
    void __tocin_join_all()
    {
        std::vector<std::thread> threads;
        {
            std::lock_guard<std::mutex> lock(g_threadsMutex);
            threads.swap(g_threads);
        }
        for (auto &t : threads)
            if (t.joinable())
                t.join();
    }
}

// ---------------------------------------------------------------------------
// Exception handling: setjmp/longjmp-based unwinding.
//
// Each `try` allocates a jmp_buf in its own frame, calls setjmp, and registers
// the buffer with __tocin_try_register. `throw` records a 64-bit value and
// longjmps back to the most recently registered handler. The handler stack is
// thread-local so goroutines have independent exception state.
// ---------------------------------------------------------------------------
namespace
{
    thread_local std::vector<std::jmp_buf *> g_handlerStack;
    thread_local int64_t g_excValue = 0;
}

extern "C"
{
    // Register a setjmp buffer as the innermost active exception handler.
    void __tocin_try_register(void *buf)
    {
        g_handlerStack.push_back(static_cast<std::jmp_buf *>(buf));
    }

    // Remove the innermost handler (try body completed without throwing).
    void __tocin_try_pop()
    {
        if (!g_handlerStack.empty())
            g_handlerStack.pop_back();
    }

    // Return the value carried by the most recently thrown exception.
    int64_t __tocin_exc_value()
    {
        return g_excValue;
    }

    // Throw: record the value and unwind to the nearest handler via longjmp.
    // With no active handler the exception is fatal.
    void __tocin_throw(int64_t value)
    {
        g_excValue = value;
        if (g_handlerStack.empty())
        {
            std::fprintf(stderr,
                         "Tocin: uncaught exception (value=%lld)\n",
                         static_cast<long long>(value));
            std::abort();
        }
        std::jmp_buf *buf = g_handlerStack.back();
        g_handlerStack.pop_back();
        std::longjmp(*buf, 1);
    }
}

// ===========================================================================
// Dynamic collections: growable vector and hashmap behind opaque handles.
// Elements/keys/values are 64-bit slots (ints stored directly; pointers/
// strings via ptrtoint round-trip on the codegen side). These leak unless
// *Free is called.
// ===========================================================================
namespace
{
    struct TocinMap
    {
        std::unordered_map<int64_t, int64_t> ints;
        std::unordered_map<std::string, int64_t> strs;
    };
}

extern "C"
{
    // ---- vector: std::vector<int64_t> ----
    void *__tocin_vec_new() { return new std::vector<int64_t>(); }
    void __tocin_vec_push(void *h, int64_t x)
    {
        if (h) static_cast<std::vector<int64_t> *>(h)->push_back(x);
    }
    int64_t __tocin_vec_get(void *h, int64_t i)
    {
        if (!h) return 0;
        auto *v = static_cast<std::vector<int64_t> *>(h);
        if (i < 0 || (size_t)i >= v->size()) return 0;
        return (*v)[(size_t)i];
    }
    void __tocin_vec_set(void *h, int64_t i, int64_t x)
    {
        if (!h) return;
        auto *v = static_cast<std::vector<int64_t> *>(h);
        if (i < 0 || (size_t)i >= v->size()) return;
        (*v)[(size_t)i] = x;
    }
    int64_t __tocin_vec_len(void *h)
    {
        return h ? (int64_t)static_cast<std::vector<int64_t> *>(h)->size() : 0;
    }
    int64_t __tocin_vec_pop(void *h)
    {
        if (!h) return 0;
        auto *v = static_cast<std::vector<int64_t> *>(h);
        if (v->empty()) return 0;
        int64_t x = v->back();
        v->pop_back();
        return x;
    }
    void __tocin_vec_free(void *h) { delete static_cast<std::vector<int64_t> *>(h); }

    // ---- hashmap: int-keyed + string-keyed entries behind one handle ----
    void *__tocin_map_new() { return new TocinMap(); }
    void __tocin_map_put(void *h, int64_t k, int64_t v)
    {
        if (h) static_cast<TocinMap *>(h)->ints[k] = v;
    }
    int64_t __tocin_map_get(void *h, int64_t k)
    {
        if (!h) return 0;
        auto *m = static_cast<TocinMap *>(h);
        auto it = m->ints.find(k);
        return it == m->ints.end() ? 0 : it->second;
    }
    int64_t __tocin_map_has(void *h, int64_t k)
    {
        return h && static_cast<TocinMap *>(h)->ints.count(k) ? 1 : 0;
    }
    void __tocin_map_put_str(void *h, const char *k, int64_t v)
    {
        if (h && k) static_cast<TocinMap *>(h)->strs[std::string(k)] = v;
    }
    int64_t __tocin_map_get_str(void *h, const char *k)
    {
        if (!h || !k) return 0;
        auto *m = static_cast<TocinMap *>(h);
        auto it = m->strs.find(std::string(k));
        return it == m->strs.end() ? 0 : it->second;
    }
    int64_t __tocin_map_has_str(void *h, const char *k)
    {
        return (h && k && static_cast<TocinMap *>(h)->strs.count(std::string(k))) ? 1 : 0;
    }
    int64_t __tocin_map_len(void *h)
    {
        if (!h) return 0;
        auto *m = static_cast<TocinMap *>(h);
        return (int64_t)(m->ints.size() + m->strs.size());
    }
    void __tocin_map_free(void *h) { delete static_cast<TocinMap *>(h); }
}

// ===========================================================================
// String runtime: char*-based, NUL-terminated. Functions returning a string
// return a fresh malloc'd buffer (never aliasing an input). Out-of-range char
// access returns -1; substring bounds are clamped; NULL inputs are tolerated.
// ===========================================================================
extern "C"
{
    static char *tocin_str_empty()
    {
        char *p = (char *)__tocin_alloc(1);
        if (p) p[0] = '\0';
        return p;
    }
    int64_t __tocin_str_len(const char *s) { return s ? (int64_t)std::strlen(s) : 0; }
    int64_t __tocin_str_char_at(const char *s, int64_t i)
    {
        if (!s) return -1;
        int64_t n = (int64_t)std::strlen(s);
        if (i < 0 || i >= n) return -1;
        return (int64_t)(unsigned char)s[i];
    }
    char *__tocin_str_substring(const char *s, int64_t start, int64_t len)
    {
        if (!s) return tocin_str_empty();
        int64_t n = (int64_t)std::strlen(s);
        if (start < 0) start = 0;
        if (start > n) start = n;
        if (len < 0) len = 0;
        if (start + len > n) len = n - start;
        char *out = (char *)__tocin_alloc((size_t)len + 1);
        if (!out) return nullptr;
        std::memcpy(out, s + start, (size_t)len);
        out[len] = '\0';
        return out;
    }
    int64_t __tocin_str_eq(const char *a, const char *b)
    {
        if (a == b) return 1;
        if (!a || !b) return 0;
        return std::strcmp(a, b) == 0 ? 1 : 0;
    }
    int64_t __tocin_str_cmp(const char *a, const char *b)
    {
        if (!a) a = "";
        if (!b) b = "";
        int r = std::strcmp(a, b);
        return r < 0 ? -1 : (r > 0 ? 1 : 0);
    }
    int64_t __tocin_str_index_of_char(const char *s, int64_t c)
    {
        if (!s) return -1;
        for (int64_t i = 0; s[i]; ++i)
            if ((unsigned char)s[i] == (unsigned char)c) return i;
        return -1;
    }
    char *__tocin_int_to_str(int64_t n)
    {
        char buf[32];
        int len = std::snprintf(buf, sizeof(buf), "%lld", (long long)n);
        if (len < 0) return tocin_str_empty();
        char *out = (char *)__tocin_alloc((size_t)len + 1);
        if (!out) return nullptr;
        std::memcpy(out, buf, (size_t)len + 1);
        return out;
    }
    int64_t __tocin_str_to_int(const char *s) { return s ? (int64_t)std::atoll(s) : 0; }
    double __tocin_str_to_float(const char *s) { return s ? std::atof(s) : 0.0; }
    char *__tocin_float_to_str(double d)
    {
        char buf[64];
        int len = std::snprintf(buf, sizeof(buf), "%g", d);
        if (len < 0) return tocin_str_empty();
        char *out = (char *)__tocin_alloc((size_t)len + 1);
        if (!out) return nullptr;
        std::memcpy(out, buf, (size_t)len + 1);
        return out;
    }
    char *__tocin_char_to_str(int64_t c)
    {
        char *out = (char *)__tocin_alloc(2);
        if (!out) return nullptr;
        out[0] = (char)(unsigned char)c;
        out[1] = '\0';
        return out;
    }
    char *__tocin_str_concat(const char *a, const char *b)
    {
        if (!a) a = "";
        if (!b) b = "";
        size_t la = std::strlen(a), lb = std::strlen(b);
        char *out = (char *)__tocin_alloc(la + lb + 1);
        if (!out) return nullptr;
        std::memcpy(out, a, la);
        std::memcpy(out + la, b, lb + 1);
        return out;
    }

    // Case conversion (fresh malloc'd copies).
    char *__tocin_str_to_upper(const char *s)
    {
        if (!s) return tocin_str_empty();
        size_t n = std::strlen(s);
        char *out = (char *)__tocin_alloc(n + 1);
        if (!out) return nullptr;
        for (size_t i = 0; i < n; ++i)
            out[i] = (char)std::toupper((unsigned char)s[i]);
        out[n] = '\0';
        return out;
    }
    char *__tocin_str_to_lower(const char *s)
    {
        if (!s) return tocin_str_empty();
        size_t n = std::strlen(s);
        char *out = (char *)__tocin_alloc(n + 1);
        if (!out) return nullptr;
        for (size_t i = 0; i < n; ++i)
            out[i] = (char)std::tolower((unsigned char)s[i]);
        out[n] = '\0';
        return out;
    }
    // Substring search: index of first occurrence, or -1.
    int64_t __tocin_str_index_of(const char *s, const char *sub)
    {
        if (!s || !sub) return -1;
        const char *p = std::strstr(s, sub);
        return p ? (int64_t)(p - s) : -1;
    }
    int64_t __tocin_str_contains(const char *s, const char *sub)
    {
        return __tocin_str_index_of(s, sub) >= 0 ? 1 : 0;
    }
    int64_t __tocin_str_starts_with(const char *s, const char *pre)
    {
        if (!s || !pre) return 0;
        size_t lp = std::strlen(pre);
        return std::strncmp(s, pre, lp) == 0 ? 1 : 0;
    }
    int64_t __tocin_str_ends_with(const char *s, const char *suf)
    {
        if (!s || !suf) return 0;
        size_t ls = std::strlen(s), lf = std::strlen(suf);
        if (lf > ls) return 0;
        return std::strcmp(s + (ls - lf), suf) == 0 ? 1 : 0;
    }

    // Explicit deallocation for programs that want to manage memory. Safe to
    // call under GC (hint to the collector); a no-op-equivalent if already
    // unreachable. Only free a buffer you own and never use it again.
    void __tocin_free(void *p)
    {
        if (!p) return;
#ifdef TOCIN_HAVE_GC
        GC_free(p);
#else
        std::free(p);
#endif
    }
}

// ===========================================================================
// File I/O. Strings are NUL-terminated char*. Returned buffers are malloc'd
// and owned by the caller; read errors yield an empty string (use len/strLen
// safely). Write/append return bytes written, or -1 on error.
// ===========================================================================
extern "C"
{
    char *__tocin_read_file(const char *path)
    {
        if (!path) return tocin_str_empty();
        FILE *f = std::fopen(path, "rb");
        if (!f) return tocin_str_empty();
        if (std::fseek(f, 0, SEEK_END) != 0) { std::fclose(f); return tocin_str_empty(); }
        long sz = std::ftell(f);
        if (sz < 0) { std::fclose(f); return tocin_str_empty(); }
        std::rewind(f);
        char *buf = (char *)__tocin_alloc((size_t)sz + 1);
        if (!buf) { std::fclose(f); return tocin_str_empty(); }
        size_t got = std::fread(buf, 1, (size_t)sz, f);
        std::fclose(f);
        buf[got] = '\0';
        return buf;
    }
    int64_t __tocin_write_file(const char *path, const char *contents)
    {
        if (!path || !contents) return -1;
        FILE *f = std::fopen(path, "wb");
        if (!f) return -1;
        size_t len = std::strlen(contents);
        size_t wrote = std::fwrite(contents, 1, len, f);
        if (std::fclose(f) != 0) return -1;
        return (wrote == len) ? (int64_t)wrote : -1;
    }
    int64_t __tocin_append_file(const char *path, const char *contents)
    {
        if (!path || !contents) return -1;
        FILE *f = std::fopen(path, "ab");
        if (!f) return -1;
        size_t len = std::strlen(contents);
        size_t wrote = std::fwrite(contents, 1, len, f);
        if (std::fclose(f) != 0) return -1;
        return (wrote == len) ? (int64_t)wrote : -1;
    }
    char *__tocin_read_line()
    {
        size_t cap = 128, n = 0;
        char *buf = (char *)__tocin_alloc(cap);
        if (!buf) return tocin_str_empty();
        int c;
        while ((c = std::fgetc(stdin)) != EOF && c != '\n')
        {
            if (n + 1 >= cap)
            {
                cap *= 2;
                char *nb = (char *)__tocin_realloc(buf, cap);
                if (!nb) { std::free(buf); return tocin_str_empty(); }
                buf = nb;
            }
            buf[n++] = (char)c;
        }
        buf[n] = '\0';
        return buf;
    }
}

// Small helper: copy n bytes into a fresh GC-managed, NUL-terminated buffer
// (__tocin_alloc is defined earlier in this file).
namespace { char *tocin_dup(const char *s, size_t n) {
    char *out = (char *)__tocin_alloc(n + 1);
    if (!out) return nullptr;
    std::memcpy(out, s, n);
    out[n] = '\0';
    return out;
} }

// ===========================================================================
// Time. Wall-clock (epoch) and a monotonic clock for measuring durations.
// ===========================================================================
extern "C"
{
    int64_t __tocin_time_sec()
    {
        using namespace std::chrono;
        return (int64_t)duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
    }
    int64_t __tocin_time_ms()
    {
        using namespace std::chrono;
        return (int64_t)duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    }
    int64_t __tocin_mono_nanos()
    {
        using namespace std::chrono;
        return (int64_t)duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
    }
    void __tocin_sleep_ms(int64_t ms)
    {
        if (ms > 0) std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }
}

// ===========================================================================
// Hashing. FNV-1a (64-bit) for strings/bytes and a splitmix64 integer mixer.
// Stable across runs — suitable for content addressing (a VCS object store),
// hash tables, and bloom filters.
// ===========================================================================
extern "C"
{
    int64_t __tocin_hash_bytes(const void *p, int64_t n)
    {
        const unsigned char *b = (const unsigned char *)p;
        uint64_t h = 1469598103934665603ULL; // FNV offset basis
        for (int64_t i = 0; b && i < n; ++i) { h ^= b[i]; h *= 1099511628211ULL; }
        return (int64_t)h;
    }
    int64_t __tocin_hash_str(const char *s)
    {
        if (!s) return (int64_t)1469598103934665603ULL;
        return __tocin_hash_bytes(s, (int64_t)std::strlen(s));
    }
    int64_t __tocin_hash_int(int64_t x)
    {
        uint64_t z = (uint64_t)x + 0x9E3779B97F4A7C15ULL;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return (int64_t)(z ^ (z >> 31));
    }
}

// ===========================================================================
// Pseudo-random numbers. A per-thread xorshift64* generator. Deterministic
// given a seed — good for simulations, sampling, and tests (not cryptography).
// ===========================================================================
namespace {
    thread_local uint64_t g_rngState = 0x853c49e6748fea9bULL;
}
extern "C"
{
    void __tocin_rand_seed(int64_t seed)
    {
        g_rngState = (uint64_t)seed ? (uint64_t)seed : 0x853c49e6748fea9bULL;
    }
    int64_t __tocin_rand_next()
    {
        uint64_t x = g_rngState;
        x ^= x >> 12; x ^= x << 25; x ^= x >> 27;
        g_rngState = x;
        uint64_t r = x * 0x2545F4914F6CDD1DULL;
        return (int64_t)(r >> 1); // non-negative
    }
    int64_t __tocin_rand_range(int64_t lo, int64_t hi)
    {
        if (hi <= lo) return lo;
        uint64_t span = (uint64_t)(hi - lo);
        return lo + (int64_t)((uint64_t)__tocin_rand_next() % span);
    }
}

// ===========================================================================
// TCP networking (POSIX sockets). File descriptors are returned as int64.
// These are the primitives a microservice or client needs; combine with `go`
// goroutines for a concurrent server. Errors return -1 / empty string.
// ===========================================================================
extern "C"
{
#ifdef TOCIN_HAVE_POSIX_NET
    int64_t __tocin_tcp_listen(int64_t port)
    {
        int fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) return -1;
        int yes = 1;
        ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
        sockaddr_in addr; std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons((uint16_t)port);
        if (::bind(fd, (sockaddr *)&addr, sizeof(addr)) < 0) { ::close(fd); return -1; }
        if (::listen(fd, 16) < 0) { ::close(fd); return -1; }
        return (int64_t)fd;
    }
    int64_t __tocin_tcp_accept(int64_t fd)
    {
        int c = ::accept((int)fd, nullptr, nullptr);
        return c < 0 ? -1 : (int64_t)c;
    }
    int64_t __tocin_tcp_connect(const char *host, int64_t port)
    {
        if (!host) return -1;
        char portstr[16]; std::snprintf(portstr, sizeof(portstr), "%lld", (long long)port);
        addrinfo hints; std::memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM;
        addrinfo *res = nullptr;
        if (::getaddrinfo(host, portstr, &hints, &res) != 0 || !res) return -1;
        int fd = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (fd < 0) { ::freeaddrinfo(res); return -1; }
        if (::connect(fd, res->ai_addr, res->ai_addrlen) < 0) { ::close(fd); ::freeaddrinfo(res); return -1; }
        ::freeaddrinfo(res);
        return (int64_t)fd;
    }
    int64_t __tocin_tcp_send(int64_t fd, const char *s)
    {
        if (!s) return 0;
        size_t len = std::strlen(s), sent = 0;
        while (sent < len)
        {
            ssize_t n = ::send((int)fd, s + sent, len - sent, 0);
            if (n <= 0) return -1;
            sent += (size_t)n;
        }
        return (int64_t)sent;
    }
    char *__tocin_tcp_recv(int64_t fd)
    {
        char buf[65536];
        ssize_t n = ::recv((int)fd, buf, sizeof(buf) - 1, 0);
        if (n <= 0) return tocin_dup("", 0);
        return tocin_dup(buf, (size_t)n);
    }
    void __tocin_tcp_close(int64_t fd) { if (fd >= 0) ::close((int)fd); }
#else
    int64_t __tocin_tcp_listen(int64_t) { return -1; }
    int64_t __tocin_tcp_accept(int64_t) { return -1; }
    int64_t __tocin_tcp_connect(const char *, int64_t) { return -1; }
    int64_t __tocin_tcp_send(int64_t, const char *) { return -1; }
    char *__tocin_tcp_recv(int64_t) { return tocin_dup("", 0); }
    void __tocin_tcp_close(int64_t) {}
#endif
}

// ===========================================================================
// Environment & process.
// ===========================================================================
extern "C"
{
    char *__tocin_env_get(const char *name)
    {
        const char *v = name ? std::getenv(name) : nullptr;
        return v ? tocin_dup(v, std::strlen(v)) : tocin_dup("", 0);
    }
    void __tocin_sys_exit(int64_t code) { std::exit((int)code); }
}
