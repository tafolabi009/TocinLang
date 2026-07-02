#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <memory>
#include <vector>
#include <set>
#include <functional>
#include <filesystem>
#include <iterator>

// Core LLVM headers
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/FileSystem.h>
// Include our LLVM shim header instead of directly including Host.h
// (the shim pulls in the real <llvm/TargetParser/Host.h> when available).
#include "llvm_shim.h"
#include <llvm/ADT/StringMap.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Scalar/GVN.h>
#include <llvm/Analysis/LoopAnalysisManager.h>
#include <llvm/Analysis/CGSCCPassManager.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Transforms/Scalar/LoopInterchange.h>
#include <llvm/Transforms/Scalar/LoopPassManager.h>
// JIT execution (ORCv2) and native object emission
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/ExecutionEngine/Orc/ExecutionUtils.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Support/CodeGen.h>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>

// Platform APIs for locating this executable (used to find the bundled linker).
#if defined(_WIN32)
// Forward-declare the single Win32 API we need rather than including <windows.h>,
// whose macros (IN, OUT, VOID, CONST, TRUE, FALSE, ERROR, ...) would clobber the
// enum members of the Tocin headers included below and break the whole build.
extern "C" __declspec(dllimport) unsigned long __stdcall
GetModuleFileNameA(void *hModule, char *lpFilename, unsigned long nSize);
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#else
#include <unistd.h>
#endif

// The Tocin runtime (channels/goroutines) lives in the static core library.
// Declared here so the JIT can register their addresses; referencing them in
// runJIT also forces the runtime objects to be linked into the compiler.
extern "C" {
    void *__tocin_chan_new();
    void __tocin_chan_send(void *, int64_t);
    int64_t __tocin_chan_recv(void *);
    void __tocin_go(void (*)(void *), void *);
    void __tocin_join_all();
    int8_t __tocin_chan_try_recv(void *, int64_t *);
    void __tocin_chan_park();
    void __tocin_try_register(void *);
    void __tocin_try_pop();
    int64_t __tocin_exc_value();
    void __tocin_throw(int64_t);
    // Dynamic collections
    void *__tocin_vec_new();
    void __tocin_vec_push(void *, int64_t);
    int64_t __tocin_vec_get(void *, int64_t);
    void __tocin_vec_set(void *, int64_t, int64_t);
    int64_t __tocin_vec_len(void *);
    int64_t __tocin_vec_pop(void *);
    void __tocin_vec_free(void *);
    void *__tocin_map_new();
    void __tocin_map_put(void *, int64_t, int64_t);
    int64_t __tocin_map_get(void *, int64_t);
    int64_t __tocin_map_has(void *, int64_t);
    void __tocin_map_put_str(void *, const char *, int64_t);
    int64_t __tocin_map_get_str(void *, const char *);
    int64_t __tocin_map_has_str(void *, const char *);
    int64_t __tocin_map_len(void *);
    void __tocin_map_free(void *);
    // Strings
    int64_t __tocin_str_len(const char *);
    int64_t __tocin_str_char_at(const char *, int64_t);
    char *__tocin_str_substring(const char *, int64_t, int64_t);
    int64_t __tocin_str_eq(const char *, const char *);
    int64_t __tocin_str_cmp(const char *, const char *);
    int64_t __tocin_str_index_of_char(const char *, int64_t);
    char *__tocin_int_to_str(int64_t);
    int64_t __tocin_str_to_int(const char *);
    char *__tocin_char_to_str(int64_t);
    char *__tocin_str_concat(const char *, const char *);
    char *__tocin_str_to_upper(const char *);
    char *__tocin_str_to_lower(const char *);
    int64_t __tocin_str_index_of(const char *, const char *);
    int64_t __tocin_str_contains(const char *, const char *);
    int64_t __tocin_str_starts_with(const char *, const char *);
    int64_t __tocin_str_ends_with(const char *, const char *);
    void __tocin_free(void *);
    void *__tocin_alloc(int64_t);
    void *__tocin_realloc(void *, int64_t);
    double __tocin_str_to_float(const char *);
    char *__tocin_float_to_str(double);
    // File I/O
    char *__tocin_read_file(const char *);
    int64_t __tocin_write_file(const char *, const char *);
    int64_t __tocin_append_file(const char *, const char *);
    char *__tocin_read_line();
    // Time
    int64_t __tocin_time_sec();
    int64_t __tocin_time_ms();
    int64_t __tocin_mono_nanos();
    void __tocin_sleep_ms(int64_t);
    // Hashing
    int64_t __tocin_hash_bytes(const void *, int64_t);
    int64_t __tocin_hash_str(const char *);
    int64_t __tocin_hash_int(int64_t);
    // Random
    void __tocin_rand_seed(int64_t);
    int64_t __tocin_rand_next();
    int64_t __tocin_rand_range(int64_t, int64_t);
    // TCP networking
    int64_t __tocin_tcp_listen(int64_t);
    int64_t __tocin_tcp_accept(int64_t);
    int64_t __tocin_tcp_connect(const char *, int64_t);
    int64_t __tocin_tcp_send(int64_t, const char *);
    char *__tocin_tcp_recv(int64_t);
    void __tocin_tcp_close(int64_t);
    // Environment / process
    char *__tocin_env_get(const char *);
    void __tocin_sys_exit(int64_t);
    // Safety
    void __tocin_oob(int64_t, int64_t);
}

#if defined(_WIN32) && defined(__GNUC__)
// On Windows/mingw the backend emits references to two libgcc/CRT helper symbols
// in generated code: __main (CRT init, called at the top of main) and
// ___chkstk_ms (the x86-64 stack-probe routine for functions with large stack
// frames). These are statically linked into this executable but are NOT
// exported, so the JIT's process-symbol generator (which uses GetProcAddress)
// cannot find them by name; we register their real addresses with the JIT below.
//
// Only symbols that actually exist in the linked libgcc/CRT may be named here:
// unlike ELF, a PE/COFF *undefined* weak reference is NOT resolved to null, so
// taking the address of a missing symbol breaks the link. These two are always
// provided by mingw-w64 on x86-64.
extern "C" {
    void __main(void) __attribute__((weak));
    void ___chkstk_ms(void) __attribute__((weak));
}
#endif

// Conditionally include Python
#ifdef WITH_PYTHON
#include <Python.h>
#endif

// Include compiler components
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "ast/ast.h"
#include "type/type_checker.h"
// Include the actual IR generator
#include "codegen/ir_generator.h"
#include "error/error_handler.h"
#include "compiler/compilation_context.h"
// Include the feature integration header
#include "type/feature_integration.h"

// New features
#include "compiler/macro_system.h"
#include "runtime/async_system.h"
#ifdef WITH_DEBUGGER
#include "debugger/debugger.h"
#endif
#ifdef WITH_WASM
#include "targets/wasm_target.h"
#endif
#ifdef WITH_PACKAGE_MANAGER
#include "package/package_manager.h"
#endif

// FFI Support
#include "ffi/ffi_interface.h"
#include "ffi/ffi_value.h"
#ifdef WITH_PYTHON
#include "ffi/ffi_python.h"
#endif
#include "ffi/ffi_cpp.h"
#include "ffi/ffi_javascript.h"

// Advanced features
#include "type/option_result_types.h"
#include "type/traits.h"
#include "type/ownership.h"
#include "type/borrow_checker.h"
#include "type/null_safety.h"
#include "type/move_semantics.h"
#include "type/extension_functions.h"
#include "runtime/concurrency.h"
#include "runtime/linq.h"

/**
 * @brief Absolute directory containing the running tocin executable.
 *
 * Used to locate the optional self-contained linker bundle (a vendored ld.lld
 * plus the CRT objects / import libs) that lets `-o <exe>` produce a native
 * binary without a system gcc/clang on PATH. Returns "" if it can't be found.
 */
static std::string tocinExecutableDir()
{
#if defined(_WIN32)
    char buf[4096];
    unsigned long n = GetModuleFileNameA(nullptr, buf, static_cast<unsigned long>(sizeof(buf)));
    if (n == 0 || n >= sizeof(buf)) return {};
    std::string p(buf, n);
#elif defined(__APPLE__)
    char buf[4096];
    uint32_t sz = sizeof(buf);
    if (_NSGetExecutablePath(buf, &sz) != 0) return {};
    std::string p(buf);
#else
    char buf[4096];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf));
    if (n <= 0) return {};
    std::string p(buf, static_cast<size_t>(n));
#endif
    auto pos = p.find_last_of("/\\");
    return pos == std::string::npos ? std::string() : p.substr(0, pos);
}

/**
 * @brief Enhanced compiler structure with all new features
 */
class EnhancedCompiler
{
public:
    EnhancedCompiler(error::ErrorHandler &errorHandler)
        : errorHandler(errorHandler), featureManager(errorHandler),
          macroSystem(std::make_unique<compiler::MacroSystem>()),
          asyncSystem(std::make_unique<runtime::AsyncSystem>())
#ifdef WITH_DEBUGGER
          ,debugger(std::make_unique<debugger::LLVMDebugger>())
#endif
#ifdef WITH_WASM
          ,wasmTarget(std::make_unique<targets::WASMTarget>())
#endif
#ifdef WITH_PACKAGE_MANAGER
          ,packageManager(std::make_unique<package::PackageManager>(".", errorHandler))
#endif
    {
        // Initialize feature manager
        featureManager.initialize();
        
        // Initialize async system
        runtime::AsyncSystem::initialize();
        
#ifdef WITH_DEBUGGER
        if (debugger) debugger->initialize();
#endif
        
        // Initialize FFI systems
        initializeFFI();
    }

    struct CompilationOptions
    {
        bool dumpIR;
        bool optimize;
        int optimizationLevel;
        std::string outputFile;
        bool enableFFI;
        bool enableConcurrency;
        bool enableAdvancedFeatures;
        bool enableMacros;
        bool enableAsync;
        bool enableDebugger;
        bool enableWASM;
        std::string target;
        bool enablePackageManager;
        bool run; // JIT-execute the program in-process (--jit / --run)
        bool freestanding; // no libc / no GC / no runtime — kernel/bare-metal
        bool noGC;         // don't link the garbage collector (alloc -> malloc)
        bool borrowCheck;  // opt-in ownership / use-after-move analysis
        bool nativeCpu;    // tune AOT codegen for the host CPU (POPCNT/AVX/...)
        bool permissive;   // do not block compilation on (non-fatal) type errors
        bool checkOnly;    // `tocin check`: stop after type checking (no codegen)

        CompilationOptions()
            : dumpIR(false), optimize(false), optimizationLevel(2), outputFile(""),
              enableFFI(true), enableConcurrency(true), enableAdvancedFeatures(true),
              enableMacros(true), enableAsync(true), enableDebugger(false),
              enableWASM(false), target("native"), enablePackageManager(true), run(false),
              freestanding(false), noGC(false), borrowCheck(false), nativeCpu(false),
              permissive(false), checkOnly(false) {}
    };

    // Exit code produced by the most recent JIT execution (--run).
    int programExitCode = 0;
    int getProgramExitCode() const { return programExitCode; }

    bool compile(const std::string &source, const std::string &filename,
                 const CompilationOptions &options = CompilationOptions())
    {
        // Lexical analysis
        lexer::Lexer lexer(source, filename, 4);
        std::vector<lexer::Token> tokens = lexer.tokenize();

        if (errorHandler.hasFatalErrors())
        {
            return false;
        }

        // Expand function-like macros at the token level, before parsing.
        if (options.enableMacros)
        {
            tokens = compiler::expandMacroTokens(tokens, errorHandler);
            if (errorHandler.hasFatalErrors())
            {
                return false;
            }
        }

        // Parsing
        parser::Parser parser(tokens);
        ast::StmtPtr program = parser.parse();

        if (errorHandler.hasFatalErrors() || !program)
        {
            return false;
        }

        // Resolve `import` statements by loading and merging other modules.
        program = resolveImports(program, filename);
        if (errorHandler.hasFatalErrors() || !program)
        {
            return false;
        }

        // Create compilation context with advanced features
        tocin::compiler::CompilationContext compilationContext(filename);

        // Type checking with advanced features
        type_checker::TypeChecker checker(errorHandler, compilationContext, &featureManager);
        checker.check(program);

        if (errorHandler.hasFatalErrors())
        {
            return false;
        }

        // Strict by default: any ERROR-severity diagnostic (undeclared
        // identifier, arity mismatch, operator/return type violation, ...)
        // blocks compilation. --permissive restores the old lenient behavior:
        // diagnostics are still printed, but codegen proceeds anyway.
        if (!options.permissive && errorHandler.hasErrors())
        {
            return false;
        }

        // `tocin check`: the front half (parse + strict type check) is the
        // whole job — perfect for editor save-hooks and CI gates. Skip codegen.
        if (options.checkOnly)
        {
            return true;
        }

        // Opt-in ownership / borrow analysis. Off by default; never changes
        // codegen. Aborts before codegen only when --borrow-check is set and an
        // ownership error is found (move/use-after-move are ERROR, not FATAL, so
        // gate on the pass's own result rather than hasFatalErrors()).
        if (options.borrowCheck)
        {
            type_checker::BorrowChecker borrowChecker(errorHandler);
            if (!borrowChecker.check(program))
                return false;
        }

        // Generate code based on target
        if (options.target == "wasm" && options.enableWASM) {
            return compileToWASM(program, filename, options);
        } else {
            return compileToNative(program, filename, options);
        }
    }

    /**
     * @brief Resolve `import` statements by loading, parsing and merging the
     *        imported modules' declarations into a single program.
     */
    ast::StmtPtr resolveImports(ast::StmtPtr program, const std::string& mainFile)
    {
        namespace fs = std::filesystem;
        std::set<std::string> loaded;
        std::vector<ast::StmtPtr> merged;

        auto flatten = [](ast::StmtPtr p) {
            std::vector<ast::StmtPtr> v;
            if (auto blk = std::dynamic_pointer_cast<ast::BlockStmt>(p)) v = blk->statements;
            else if (auto mod = std::dynamic_pointer_cast<ast::ModuleStmt>(p)) v = mod->body;
            else if (p) v.push_back(p);
            return v;
        };

        auto resolvePath = [&](const std::string& name, const std::string& fromDir) -> std::string {
            std::string rel = name;
            if (rel.size() < 3 || rel.substr(rel.size() - 3) != ".to") rel += ".to";
            std::vector<std::string> candidates;
            if (!fromDir.empty()) candidates.push_back((fs::path(fromDir) / rel).string());
            if (const char* envp = std::getenv("TOCIN_PATH"))
                candidates.push_back((fs::path(envp) / rel).string());
#ifdef TOCIN_STDLIB_PATH
            candidates.push_back((fs::path(TOCIN_STDLIB_PATH) / rel).string());
#endif
            for (auto& c : candidates)
            {
                std::error_code ec;
                if (fs::exists(c, ec)) return fs::absolute(c, ec).string();
            }
            return "";
        };

        std::function<void(ast::StmtPtr, const std::string&)> process =
            [&](ast::StmtPtr prog, const std::string& fromDir) {
            for (auto& s : flatten(prog))
            {
                if (auto imp = std::dynamic_pointer_cast<ast::ImportStmt>(s))
                {
                    std::string file = resolvePath(imp->moduleName, fromDir);
                    if (file.empty())
                    {
                        errorHandler.reportError(error::ErrorCode::I001_FILE_NOT_FOUND,
                                                 "Cannot resolve import '" + imp->moduleName + "'",
                                                 mainFile, 0, 0);
                        continue;
                    }
                    if (loaded.count(file)) continue;
                    loaded.insert(file);
                    std::ifstream f(file);
                    std::string src((std::istreambuf_iterator<char>(f)),
                                    std::istreambuf_iterator<char>());
                    lexer::Lexer lx(src, file, 4);
                    auto toks = lx.tokenize();
                    parser::Parser ps(toks);
                    auto sub = ps.parse();
                    process(sub, fs::path(file).parent_path().string()); // imported module's imports first
                    // Keep the import statement itself in the merged program:
                    // codegen ignores it, but the type checker uses it to learn
                    // module aliases (`import math.basic` makes `math` legal in
                    // qualified calls like math.add(...)).
                    merged.push_back(s);
                }
                else if (s)
                {
                    merged.push_back(s);
                }
            }
        };

        process(program, fs::path(mainFile).parent_path().string());
        return std::make_shared<ast::BlockStmt>(
            lexer::Token(lexer::TokenType::IDENTIFIER, "", mainFile, 0, 0), merged);
    }

    bool compileToNative(ast::StmtPtr program, const std::string& filename,
                        const CompilationOptions& options)
    {
        // IR generation. The context is heap-allocated so it can be handed to
        // the JIT (which needs to own the context alongside the module).
        auto context = std::make_unique<llvm::LLVMContext>();
        auto module = std::make_unique<llvm::Module>(filename, *context);

        useNativeCpu_ = options.nativeCpu;   // read by createConfiguredTargetMachine()

        // Give the module its real triple and data layout BEFORE IR generation.
        // Alignment is stamped on allocas/loads/stores at creation time, so an
        // empty layout here left i64 accesses with the default ABI alignment of
        // 4 (visible as `align 4` on 8-byte loads in the emitted code), which
        // penalizes the vectorizer. Doing it up front also lets every middle-end
        // cost model see the real pointer/ABI sizes.
        if (auto tm = createConfiguredTargetMachine())
        {
#if LLVM_VERSION_MAJOR >= 21
            module->setTargetTriple(tm->getTargetTriple());
#else
            module->setTargetTriple(tm->getTargetTriple().str());
#endif
            module->setDataLayout(tm->createDataLayout());
        }

        codegen::IRGenerator generator(*context, std::move(module), errorHandler);
        generator.freestanding = options.freestanding;

        // Generate LLVM IR from the AST
        auto generatedModule = generator.generate(program);
        if (errorHandler.hasFatalErrors() || !generatedModule)
        {
            return false;
        }

        // Verify the module
        std::string verifierErrors;
        llvm::raw_string_ostream verifierStream(verifierErrors);
        if (llvm::verifyModule(*generatedModule, &verifierStream))
        {
            errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                     "Invalid LLVM IR generated: " + verifierErrors,
                                     filename, 0, 0);
            return false;
        }

        // Whole-program view: a self-contained executable (or a JIT run) has
        // main as its only entry point, so every other definition can be
        // internalized. This is what unlocks cross-function -O3 - full inlining
        // freedom, argument specialization, and dead-code elimination
        // (LTO-style, in one module). Skipped for .ll/.s/.o outputs (they may
        // be linked against other objects) and --freestanding (the kernel's
        // entry symbols must stay visible to the external linker script).
        auto hasSuffix = [](const std::string& s, const std::string& suffix) {
            return s.size() >= suffix.size() &&
                   s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
        };
        const std::string& outPath = options.outputFile;
        const bool partialOutput = hasSuffix(outPath, ".ll") || hasSuffix(outPath, ".s") ||
                                   hasSuffix(outPath, ".o") || hasSuffix(outPath, ".obj");
        const bool wholeProgram = !options.freestanding &&
                                  (options.run || (!outPath.empty() && !partialOutput));
        if (wholeProgram)
        {
            for (auto &F : *generatedModule)
                if (!F.isDeclaration() && F.hasExternalLinkage() && F.getName() != "main")
                    F.setLinkage(llvm::GlobalValue::InternalLinkage);
            for (auto &G : generatedModule->globals())
                if (G.hasInitializer() && G.hasExternalLinkage() &&
                    !G.getName().starts_with("llvm."))
                    G.setLinkage(llvm::GlobalValue::InternalLinkage);
        }

        // Optimize if requested
        if (options.optimize && !errorHandler.hasFatalErrors())
        {
            optimizeModule(*generatedModule, options.optimizationLevel);
        }

        // Dump IR if requested
        if (options.dumpIR)
        {
            std::string irOutput;
            llvm::raw_string_ostream irStream(irOutput);
            irStream << *generatedModule;
            std::cout << irOutput << std::endl;
        }

        // JIT execution takes precedence: run the program in-process.
        if (options.run)
        {
            if (options.freestanding)
            {
                errorHandler.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                    "--run/--jit is not supported with --freestanding (no host runtime); "
                    "emit an object with -o and link it into your kernel",
                    filename, 0, 0);
                return false;
            }
            return runJIT(std::move(context), std::move(generatedModule), filename);
        }

        // Write output if specified.
        if (!options.outputFile.empty())
        {
            const std::string& outputPath = options.outputFile;
            auto endsWith = [](const std::string& s, const std::string& suffix) {
                return s.size() >= suffix.size() &&
                       s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
            };

            if (endsWith(outputPath, ".ll"))
            {
                std::error_code EC;
                llvm::raw_fd_ostream out(outputPath, EC);
                if (EC)
                {
                    errorHandler.reportError(error::ErrorCode::I003_READ_ERROR,
                                             "Could not open output file: " + EC.message(),
                                             filename, 0, 0);
                    return false;
                }
                out << *generatedModule;
            }
            else if (endsWith(outputPath, ".s"))
            {
                if (!emitObjectFile(*generatedModule, outputPath, /*asAssembly=*/true))
                    return false;
            }
            else if (endsWith(outputPath, ".o"))
            {
                if (!emitObjectFile(*generatedModule, outputPath, /*asAssembly=*/false))
                    return false;
            }
            else if (options.freestanding)
            {
                // Freestanding/kernel: emit a relocatable object only. The kernel
                // author links it (with their own _start, allocator, and linker
                // script); we do NOT pull in libc/GC/runtime.
                std::string objPath = endsWith(outputPath, ".obj") ? outputPath : outputPath + ".o";
                if (!emitObjectFile(*generatedModule, objPath, false))
                    return false;
                std::cout << "Freestanding object written: " << objPath
                          << " (link it into your kernel; provides symbol 'main')\n";
            }
            else
            {
                // Produce a native executable: emit a temporary object file
                // and link it with the system C toolchain.
                std::string objPath = outputPath + ".o";
                if (!emitObjectFile(*generatedModule, objPath, false))
                    return false;
                if (!linkExecutable(objPath, outputPath))
                    return false;
                std::remove(objPath.c_str());
            }
        }

        return !errorHandler.hasFatalErrors();
    }

    /**
     * @brief JIT-compile and execute the module's main() in-process.
     */
    bool runJIT(std::unique_ptr<llvm::LLVMContext> context,
                std::unique_ptr<llvm::Module> module,
                const std::string& filename)
    {
        auto jitOrErr = llvm::orc::LLJITBuilder().create();
        if (!jitOrErr)
        {
            errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                     "Failed to create JIT: " +
                                         llvm::toString(jitOrErr.takeError()),
                                     filename, 0, 0);
            return false;
        }
        auto jit = std::move(*jitOrErr);

        // Resolve external symbols (printf, malloc, ...) from this process.
        if (auto gen = llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(
                jit->getDataLayout().getGlobalPrefix()))
        {
            jit->getMainJITDylib().addGenerator(std::move(*gen));
        }

        // Register the Tocin runtime (channels/goroutines) so JIT'd code can
        // call it. Taking the addresses also forces these symbols to be linked.
        {
            llvm::orc::SymbolMap rt;
            auto def = [&](const char *name, void *addr) {
                rt[jit->mangleAndIntern(name)] = llvm::orc::ExecutorSymbolDef(
                    llvm::orc::ExecutorAddr::fromPtr(addr), llvm::JITSymbolFlags::Exported);
            };
            def("__tocin_chan_new", reinterpret_cast<void *>(&__tocin_chan_new));
            def("__tocin_chan_send", reinterpret_cast<void *>(&__tocin_chan_send));
            def("__tocin_chan_recv", reinterpret_cast<void *>(&__tocin_chan_recv));
            def("__tocin_go", reinterpret_cast<void *>(&__tocin_go));
            def("__tocin_join_all", reinterpret_cast<void *>(&__tocin_join_all));
            def("__tocin_chan_try_recv", reinterpret_cast<void *>(&__tocin_chan_try_recv));
            def("__tocin_chan_park", reinterpret_cast<void *>(&__tocin_chan_park));
            def("__tocin_try_register", reinterpret_cast<void *>(&__tocin_try_register));
            def("__tocin_try_pop", reinterpret_cast<void *>(&__tocin_try_pop));
            def("__tocin_exc_value", reinterpret_cast<void *>(&__tocin_exc_value));
            def("__tocin_throw", reinterpret_cast<void *>(&__tocin_throw));
            def("__tocin_vec_new", reinterpret_cast<void *>(&__tocin_vec_new));
            def("__tocin_vec_push", reinterpret_cast<void *>(&__tocin_vec_push));
            def("__tocin_vec_get", reinterpret_cast<void *>(&__tocin_vec_get));
            def("__tocin_vec_set", reinterpret_cast<void *>(&__tocin_vec_set));
            def("__tocin_vec_len", reinterpret_cast<void *>(&__tocin_vec_len));
            def("__tocin_vec_pop", reinterpret_cast<void *>(&__tocin_vec_pop));
            def("__tocin_vec_free", reinterpret_cast<void *>(&__tocin_vec_free));
            def("__tocin_map_new", reinterpret_cast<void *>(&__tocin_map_new));
            def("__tocin_map_put", reinterpret_cast<void *>(&__tocin_map_put));
            def("__tocin_map_get", reinterpret_cast<void *>(&__tocin_map_get));
            def("__tocin_map_has", reinterpret_cast<void *>(&__tocin_map_has));
            def("__tocin_map_put_str", reinterpret_cast<void *>(&__tocin_map_put_str));
            def("__tocin_map_get_str", reinterpret_cast<void *>(&__tocin_map_get_str));
            def("__tocin_map_has_str", reinterpret_cast<void *>(&__tocin_map_has_str));
            def("__tocin_map_len", reinterpret_cast<void *>(&__tocin_map_len));
            def("__tocin_map_free", reinterpret_cast<void *>(&__tocin_map_free));
            def("__tocin_str_len", reinterpret_cast<void *>(&__tocin_str_len));
            def("__tocin_str_char_at", reinterpret_cast<void *>(&__tocin_str_char_at));
            def("__tocin_str_substring", reinterpret_cast<void *>(&__tocin_str_substring));
            def("__tocin_str_eq", reinterpret_cast<void *>(&__tocin_str_eq));
            def("__tocin_str_cmp", reinterpret_cast<void *>(&__tocin_str_cmp));
            def("__tocin_str_index_of_char", reinterpret_cast<void *>(&__tocin_str_index_of_char));
            def("__tocin_int_to_str", reinterpret_cast<void *>(&__tocin_int_to_str));
            def("__tocin_str_to_int", reinterpret_cast<void *>(&__tocin_str_to_int));
            def("__tocin_char_to_str", reinterpret_cast<void *>(&__tocin_char_to_str));
            def("__tocin_str_concat", reinterpret_cast<void *>(&__tocin_str_concat));
            def("__tocin_str_to_upper", reinterpret_cast<void *>(&__tocin_str_to_upper));
            def("__tocin_str_to_lower", reinterpret_cast<void *>(&__tocin_str_to_lower));
            def("__tocin_str_index_of", reinterpret_cast<void *>(&__tocin_str_index_of));
            def("__tocin_str_contains", reinterpret_cast<void *>(&__tocin_str_contains));
            def("__tocin_str_starts_with", reinterpret_cast<void *>(&__tocin_str_starts_with));
            def("__tocin_str_ends_with", reinterpret_cast<void *>(&__tocin_str_ends_with));
            def("__tocin_free", reinterpret_cast<void *>(&__tocin_free));
            def("__tocin_alloc", reinterpret_cast<void *>(&__tocin_alloc));
            def("__tocin_realloc", reinterpret_cast<void *>(&__tocin_realloc));
            def("__tocin_str_to_float", reinterpret_cast<void *>(&__tocin_str_to_float));
            def("__tocin_float_to_str", reinterpret_cast<void *>(&__tocin_float_to_str));
            def("__tocin_read_file", reinterpret_cast<void *>(&__tocin_read_file));
            def("__tocin_write_file", reinterpret_cast<void *>(&__tocin_write_file));
            def("__tocin_append_file", reinterpret_cast<void *>(&__tocin_append_file));
            def("__tocin_read_line", reinterpret_cast<void *>(&__tocin_read_line));
            def("__tocin_time_sec", reinterpret_cast<void *>(&__tocin_time_sec));
            def("__tocin_time_ms", reinterpret_cast<void *>(&__tocin_time_ms));
            def("__tocin_mono_nanos", reinterpret_cast<void *>(&__tocin_mono_nanos));
            def("__tocin_sleep_ms", reinterpret_cast<void *>(&__tocin_sleep_ms));
            def("__tocin_hash_bytes", reinterpret_cast<void *>(&__tocin_hash_bytes));
            def("__tocin_hash_str", reinterpret_cast<void *>(&__tocin_hash_str));
            def("__tocin_hash_int", reinterpret_cast<void *>(&__tocin_hash_int));
            def("__tocin_rand_seed", reinterpret_cast<void *>(&__tocin_rand_seed));
            def("__tocin_rand_next", reinterpret_cast<void *>(&__tocin_rand_next));
            def("__tocin_rand_range", reinterpret_cast<void *>(&__tocin_rand_range));
            def("__tocin_tcp_listen", reinterpret_cast<void *>(&__tocin_tcp_listen));
            def("__tocin_tcp_accept", reinterpret_cast<void *>(&__tocin_tcp_accept));
            def("__tocin_tcp_connect", reinterpret_cast<void *>(&__tocin_tcp_connect));
            def("__tocin_tcp_send", reinterpret_cast<void *>(&__tocin_tcp_send));
            def("__tocin_tcp_recv", reinterpret_cast<void *>(&__tocin_tcp_recv));
            def("__tocin_tcp_close", reinterpret_cast<void *>(&__tocin_tcp_close));
            def("__tocin_env_get", reinterpret_cast<void *>(&__tocin_env_get));
            def("__tocin_sys_exit", reinterpret_cast<void *>(&__tocin_sys_exit));
            def("__tocin_oob", reinterpret_cast<void *>(&__tocin_oob));
            llvm::cantFail(jit->getMainJITDylib().define(llvm::orc::absoluteSymbols(std::move(rt))));
        }

#if defined(_WIN32) && defined(__GNUC__)
        // Provide the mingw/libgcc helper symbols the backend references in
        // generated code (see the weak declarations above). Only the ones the
        // linked libgcc actually defines are non-null; register those.
        {
            llvm::orc::SymbolMap mingwrt;
            auto defabs = [&](const char *name, void *addr) {
                if (!addr) return;
                mingwrt[jit->mangleAndIntern(name)] = llvm::orc::ExecutorSymbolDef(
                    llvm::orc::ExecutorAddr::fromPtr(addr), llvm::JITSymbolFlags::Exported);
            };
            defabs("__main", reinterpret_cast<void *>(&__main));
            defabs("___chkstk_ms", reinterpret_cast<void *>(&___chkstk_ms));
            if (!mingwrt.empty())
                llvm::cantFail(jit->getMainJITDylib().define(
                    llvm::orc::absoluteSymbols(std::move(mingwrt))));
        }
#endif

        if (auto err = jit->addIRModule(
                llvm::orc::ThreadSafeModule(std::move(module), std::move(context))))
        {
            errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                     "Failed to add module to JIT: " +
                                         llvm::toString(std::move(err)),
                                     filename, 0, 0);
            return false;
        }

        auto mainSym = jit->lookup("main");
        if (!mainSym)
        {
            errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                     "No 'main' function to execute: " +
                                         llvm::toString(mainSym.takeError()),
                                     filename, 0, 0);
            return false;
        }

        auto* mainFn = mainSym->toPtr<int64_t (*)()>();
        programExitCode = static_cast<int>(mainFn());
        return true;
    }

    /**
     * @brief Build a TargetMachine for the host, configured for the current mode.
     *
     * The CPU + feature string drives BOTH halves of the compiler:
     *   - the middle-end (via the PassBuilder's TargetIRAnalysis/TTI in
     *     optimizeModule) - so LoopIdiomRecognize can fold the Kernighan popcount
     *     loop into @llvm.ctpop and the loop/SLP vectorizers know which vector
     *     ISA is available;
     *   - the backend (instruction selection in emitObjectFile).
     * Default CPU "generic" keeps binaries portable. With --native we pin the
     * build host's CPU and full feature set (POPCNT, BMI, AVX/AVX2, FMA, ...),
     * the equivalent of clang's -march=native. Returns nullptr on lookup failure.
     */
    std::unique_ptr<llvm::TargetMachine> createConfiguredTargetMachine()
    {
        std::string triple = llvm::sys::getDefaultTargetTriple();
        std::string error;
#if LLVM_VERSION_MAJOR >= 21
        const llvm::Target* target = llvm::TargetRegistry::lookupTarget(llvm::Triple(triple), error);
#else
        const llvm::Target* target = llvm::TargetRegistry::lookupTarget(triple, error);
#endif
        if (!target)
            return nullptr;

        std::string cpu = "generic";
        std::string features;
        if (useNativeCpu_)
        {
            cpu = llvm::sys::getHostCPUName().str();
#if LLVM_VERSION_MAJOR >= 19
            llvm::StringMap<bool> hostFeatures = llvm::sys::getHostCPUFeatures();
#else
            llvm::StringMap<bool> hostFeatures;
            llvm::sys::getHostCPUFeatures(hostFeatures);
#endif
            for (auto& f : hostFeatures)
            {
                if (!features.empty()) features += ",";
                features += (f.second ? "+" : "-");
                features += f.first().str();
            }
        }

        llvm::TargetOptions opt;
        // LLVM 21 changed createTargetMachine's first parameter from a triple
        // string to an llvm::Triple. Guard so the same source builds on 18-22.
#if LLVM_VERSION_MAJOR >= 21
        llvm::TargetMachine* tm = target->createTargetMachine(
            llvm::Triple(triple), cpu, features, opt, std::optional<llvm::Reloc::Model>(llvm::Reloc::PIC_));
#else
        llvm::TargetMachine* tm = target->createTargetMachine(
            triple, cpu, features, opt, std::optional<llvm::Reloc::Model>(llvm::Reloc::PIC_));
#endif
        return std::unique_ptr<llvm::TargetMachine>(tm);
    }

    /**
     * @brief Emit a native object (or assembly) file from the module.
     */
    bool emitObjectFile(llvm::Module& module, const std::string& outputPath, bool asAssembly)
    {
        std::string triple = llvm::sys::getDefaultTargetTriple();
        // LLVM 21 changed Module::setTargetTriple to take an llvm::Triple.
#if LLVM_VERSION_MAJOR >= 21
        module.setTargetTriple(llvm::Triple(triple));
#else
        module.setTargetTriple(triple);
#endif

        auto targetMachine = createConfiguredTargetMachine();
        if (!targetMachine)
        {
            errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                     "Failed to create target machine for " + triple, "", 0, 0);
            return false;
        }
        module.setDataLayout(targetMachine->createDataLayout());

        std::error_code EC;
        llvm::raw_fd_ostream dest(outputPath, EC, llvm::sys::fs::OF_None);
        if (EC)
        {
            errorHandler.reportError(error::ErrorCode::I003_READ_ERROR,
                                     "Could not open output file: " + EC.message(), "", 0, 0);
            return false;
        }

        llvm::legacy::PassManager pass;
        auto fileType = asAssembly ? llvm::CodeGenFileType::AssemblyFile
                                   : llvm::CodeGenFileType::ObjectFile;
        if (targetMachine->addPassesToEmitFile(pass, dest, nullptr, fileType))
        {
            errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                     "Target machine cannot emit this file type", "", 0, 0);
            return false;
        }
        pass.run(module);
        dest.flush();
        return true;
    }

    /**
     * @brief Link an object file into a native executable using the system C toolchain.
     */
    // Result of attempting the self-contained bundled linker.
    enum class BundledLink { Ok, Failed, NotBundled };

    // Link <objPath> -> <exePath> using a vendored ld.lld driven by a data-driven
    // recipe shipped next to the compiler (libexec/link/). The recipe is an
    // ld.lld argument list (whitespace/newline separated) with placeholders:
    //   %LINKDIR% -> absolute path of the link/ bundle dir
    //   %OBJ%     -> the input object
    //   %OUT%     -> the output executable
    // Returns NotBundled when no bundle is present (caller falls back to the C
    // driver), Ok on success, or Failed (error reported) when the bundled linker
    // is present but the link fails. Keeping the recipe as data means the link
    // line can be corrected without rebuilding the compiler.
    BundledLink tryBundledLink(const std::string& objPath, const std::string& exePath)
    {
        namespace fs = std::filesystem;
        std::string dir = tocinExecutableDir();
        if (dir.empty()) return BundledLink::NotBundled;
        fs::path linkDir = fs::path(dir) / "link";
#if defined(_WIN32)
        fs::path lld = linkDir / "ld.lld.exe";
#else
        fs::path lld = linkDir / "ld.lld";
#endif
        fs::path recipePath = linkDir / "link-recipe.txt";
        std::error_code ec;
        if (!fs::exists(lld, ec) || !fs::exists(recipePath, ec))
            return BundledLink::NotBundled;

        std::ifstream in(recipePath);
        if (!in) return BundledLink::NotBundled;
        std::stringstream buf;
        buf << in.rdbuf();
        std::string args = buf.str();

        auto substAll = [&](const std::string& key, const std::string& val) {
            for (size_t p = args.find(key); p != std::string::npos;
                 p = args.find(key, p + val.size()))
                args.replace(p, key.size(), val);
        };
        substAll("%LINKDIR%", linkDir.string());
        substAll("%OBJ%", objPath);
        substAll("%OUT%", exePath);

        // Pass the (possibly long) argument list via an @response file so command
        // length limits and path quoting are not a concern.
        fs::path resp = fs::path(exePath).parent_path() / ".tocin-link.rsp";
        {
            std::ofstream o(resp);
            o << args;
        }
        // The bundled ld.lld is dynamically linked against the same shared libs
        // (libLLVM, libstdc++, ...) that sit next to tocin in libexec. Prepend
        // our own directory to PATH for the child so it finds them, instead of
        // duplicating ~100MB of DLLs into link/.
        std::string oldPath = std::getenv("PATH") ? std::getenv("PATH") : "";
#if defined(_WIN32)
        _putenv_s("PATH", (dir + ";" + oldPath).c_str());
#else
        std::string oldLd = std::getenv("LD_LIBRARY_PATH") ? std::getenv("LD_LIBRARY_PATH") : "";
        setenv("PATH", (dir + ":" + oldPath).c_str(), 1);
        setenv("LD_LIBRARY_PATH", (dir + ":" + oldLd).c_str(), 1);
#endif
        std::string cmd = "\"" + lld.string() + "\" @\"" + resp.string() + "\"";
        int rc = std::system(cmd.c_str());
#if defined(_WIN32)
        _putenv_s("PATH", oldPath.c_str());
#else
        setenv("PATH", oldPath.c_str(), 1);
#endif
        fs::remove(resp, ec);
        if (rc != 0) {
            errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                     "Bundled linker (ld.lld) failed (exit " +
                                         std::to_string(rc) + "). Recipe: " +
                                         recipePath.string(),
                                     "", 0, 0);
            return BundledLink::Failed;
        }
        return BundledLink::Ok;
    }

    bool linkExecutable(const std::string& objPath, const std::string& exePath)
    {
        // Prefer the self-contained bundled linker (vendored ld.lld + CRT/import
        // libs) so native output needs no external gcc/clang. Fall through to the
        // C driver only when no bundle is present.
        switch (tryBundledLink(objPath, exePath)) {
            case BundledLink::Ok:         return true;
            case BundledLink::Failed:     return false;
            case BundledLink::NotBundled: break;
        }

        // Pick a C toolchain driver to link with. $CC wins; otherwise probe the
        // usual driver names and use the first that actually runs. mingw-w64
        // ships 'gcc' (there is no 'cc'), so it is tried first on Windows.
        std::string cc;
        if (const char* env = std::getenv("CC"); env && *env) {
            cc = env;
        } else {
#if defined(_WIN32)
            const char* devnull = "NUL";
            const char* candidates[] = {"gcc", "clang", "cc"};
#else
            const char* devnull = "/dev/null";
            const char* candidates[] = {"cc", "gcc", "clang"};
#endif
            for (const char* cand : candidates) {
                std::string probe = std::string(cand) + " --version >" + devnull + " 2>&1";
                if (std::system(probe.c_str()) == 0) { cc = cand; break; }
            }
        }
        if (cc.empty()) {
            errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                     "No C toolchain driver found to link the native executable. "
                                     "Install a C compiler (Windows: mingw-w64 'gcc'; Linux/macOS: "
                                     "gcc or clang) and put it on PATH, or set the CC environment "
                                     "variable. (Tip: 'tocin file.to --run' needs no external tools.)",
                                     "", 0, 0);
            return false;
        }
        std::string cmd = cc + " \"" + objPath + "\" -o \"" + exePath + "\"";
        // Link the Tocin runtime (channels/goroutines) so programs that use
        // concurrency resolve their __tocin_* calls.
#ifdef TOCIN_RUNTIME_LIB
        cmd += std::string(" \"") + TOCIN_RUNTIME_LIB + "\"";
#endif
        // Link the garbage collector so collected allocations resolve at runtime.
#ifdef TOCIN_GC_LIB
        cmd += std::string(" \"") + TOCIN_GC_LIB + "\"";
#endif
        cmd += " -lm -lpthread -lstdc++";
        int rc = std::system(cmd.c_str());
        if (rc != 0)
        {
            errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                     "Linker failed (exit " + std::to_string(rc) +
                                         "): " + cmd,
                                     "", 0, 0);
            return false;
        }
        return true;
    }

    bool compileToWASM(ast::StmtPtr program, const std::string& filename,
                       const CompilationOptions& options)
    {
#ifdef WITH_WASM
        targets::WASMTargetConfig config;
        config.optimize = options.optimize;
        config.enableSIMD = true;
        config.enableExceptionHandling = true;
        
        auto target = std::make_unique<targets::WASMTarget>(config);
        std::string wasmCode = target->generateWASM(program, errorHandler);
        
        if (errorHandler.hasFatalErrors() || wasmCode.empty())
        {
            return false;
        }

        // Optimize WASM if requested
        if (options.optimize) {
            wasmCode = target->optimizeWASM(wasmCode);
        }

        // Validate WASM
        if (!target->validateWASM(wasmCode, errorHandler)) {
            return false;
        }

        // Write WASM output
        if (!options.outputFile.empty()) {
            std::string outputPath = options.outputFile;
            if (outputPath.find('.') == std::string::npos) {
                outputPath += ".wasm";
            }

            std::ofstream outputFile(outputPath);
            if (!outputFile.is_open()) {
                errorHandler.reportError(error::ErrorCode::I003_READ_ERROR,
                                       "Could not open output file: " + outputPath,
                                       filename, 0, 0);
                return false;
            }

            outputFile << wasmCode;
        }

        return !errorHandler.hasFatalErrors();
#else
        (void)program; (void)filename; (void)options;
        return false;
#endif
    }

    // Package manager methods
    bool installPackage(const std::string& name, const std::string& version = "") {
#ifdef WITH_PACKAGE_MANAGER
        return packageManager->install(name, version);
#else
        return false;
#endif
    }

    bool uninstallPackage(const std::string& name) {
#ifdef WITH_PACKAGE_MANAGER
        return packageManager->uninstall(name);
#else
        return false;
#endif
    }

#ifdef WITH_PACKAGE_MANAGER
    std::vector<package::PackageInfo> searchPackages(const std::string& query) {
        return packageManager->search(query);
    }
#endif

    // Debugger methods
    void startDebugger() {
#ifdef WITH_DEBUGGER
        if (debugger) {
            debugger->start();
        }
#endif
    }

    void setBreakpoint(const std::string& filename, int line, int column = 0) {
#ifdef WITH_DEBUGGER
        if (debugger) {
            debugger->setBreakpoint(filename, line, column);
        }
#endif
    }

    void stepInto() {
#ifdef WITH_DEBUGGER
        if (debugger) {
            debugger->stepInto();
        }
#endif
    }

    void stepOver() {
#ifdef WITH_DEBUGGER
        if (debugger) {
            debugger->stepOver();
        }
#endif
    }

    void continueExecution() {
#ifdef WITH_DEBUGGER
        if (debugger) {
            debugger->continueExecution();
        }
#endif
    }

    // Async methods
    template<typename T>
    runtime::Future<T> createAsync(std::function<T()> func) {
        return runtime::AsyncSystem::createAsync(func).execute();
    }

    template<typename T>
    T await(runtime::Future<T>& future) {
        return runtime::AsyncSystem::await(future);
    }

private:
    error::ErrorHandler &errorHandler;
    bool useNativeCpu_ = false;   // --native: tune AOT codegen for the host CPU
    type_checker::FeatureManager featureManager;
    std::unique_ptr<compiler::MacroSystem> macroSystem;
    std::unique_ptr<runtime::AsyncSystem> asyncSystem;
#ifdef WITH_DEBUGGER
    std::unique_ptr<debugger::Debugger> debugger;
#endif
#ifdef WITH_WASM
    std::unique_ptr<targets::WASMTarget> wasmTarget;
#endif
#ifdef WITH_PACKAGE_MANAGER
    std::unique_ptr<package::PackageManager> packageManager;
#endif

    void initializeFFI()
    {
        // FFI initialization is handled by the relevant modules if available
    }

    std::string processMacros(const std::string& source, const std::string& filename) {
        // Process macros in the source code
        // This would expand macros before compilation
        return source; // Placeholder
    }

    void optimizeModule(llvm::Module &module, int level)
    {
        // A TargetMachine threads host-CPU info (TargetTransformInfo) into the
        // middle-end. Without it the optimizer assumes a baseline CPU, so the
        // popcount idiom never lowers to @llvm.ctpop and the vectorizers don't
        // know which vector ISA exists - which is why --native previously only
        // affected the backend and produced no speedup. Targeting the module
        // (triple + data layout) up front also gives the cost models accurate
        // pointer/ABI sizes. A null TM (lookup failure) degrades to the old
        // target-neutral behavior, which PassBuilder handles.
        auto targetMachine = createConfiguredTargetMachine();
        if (targetMachine)
        {
#if LLVM_VERSION_MAJOR >= 21
            module.setTargetTriple(llvm::Triple(llvm::sys::getDefaultTargetTriple()));
#else
            module.setTargetTriple(llvm::sys::getDefaultTargetTriple());
#endif
            module.setDataLayout(targetMachine->createDataLayout());
        }

        // Create a function pass manager
        llvm::PassBuilder passBuilder(targetMachine.get());

        // GCC turns the classic i-j-k matmul into a vectorizable kernel with
        // loop interchange at -O3; LLVM ships the pass but keeps it out of the
        // default pipeline. Run it right before the vectorizers at -O3 so
        // column-strided inner loops become contiguous and vectorize.
        if (level >= 3)
        {
            passBuilder.registerVectorizerStartEPCallback(
                [](llvm::FunctionPassManager &FPM, llvm::OptimizationLevel) {
                    FPM.addPass(llvm::createFunctionToLoopPassAdaptor(
                        llvm::LoopInterchangePass()));
                });
        }

        llvm::LoopAnalysisManager LAM;
        llvm::FunctionAnalysisManager FAM;
        llvm::CGSCCAnalysisManager CGAM;
        llvm::ModuleAnalysisManager MAM;

        // Register all the basic analyses with the managers
        passBuilder.registerModuleAnalyses(MAM);
        passBuilder.registerCGSCCAnalyses(CGAM);
        passBuilder.registerFunctionAnalyses(FAM);
        passBuilder.registerLoopAnalyses(LAM);
        passBuilder.crossRegisterProxies(LAM, FAM, CGAM, MAM);

        // Create the optimization pipeline based on optimization level
        llvm::ModulePassManager MPM;
        if (level == 0)
        {
            // O0 - No optimization
            MPM = passBuilder.buildO0DefaultPipeline(llvm::OptimizationLevel::O0);
        }
        else if (level == 1)
        {
            // O1 - Basic optimizations
            MPM = passBuilder.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O1);
        }
        else if (level == 2)
        {
            // O2 - Default optimizations
            MPM = passBuilder.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O2);
        }
        else if (level == 3)
        {
            // O3 - Aggressive optimizations
            MPM = passBuilder.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O3);
        }

        // Run the optimizations
        MPM.run(module, MAM);
    }
};

/**
 * @brief Displays usage information.
 */
void displayUsage()
{
    std::cout << "Usage: tocin [options] [filename]\n"
              << "       tocin check <file.to>   Typecheck only (no codegen); exit 0 if clean\n"
              << "       tocin new <name>        Scaffold a new project directory\n"
              << "       tocin doc <file.to>     Generate Markdown API docs to stdout\n"
              << "Options:\n"
              << "  --help, -h             Display this help message\n"
              << "  --version, -V          Print the compiler version and exit\n"
              << "  --dump-ir              Dump LLVM IR to stdout\n"
              << "  --jit, --run           JIT-compile and run the program immediately\n"
              << "  -O0, -O1, -O2, -O3     Set optimization level (default: -O2)\n"
              << "  -o <file>              Write output to <file>. Extension selects format:\n"
              << "                           .ll = LLVM IR, .s = assembly, .o = object,\n"
              << "                           anything else = native executable\n"
              << "  --target <target>      Set compilation target (native, wasm)\n"
              << "  --borrow-check         Enable opt-in ownership / use-after-move checking\n"
              << "  --native               Tune native output for this CPU (POPCNT/AVX/...); not portable\n"
              << "  --permissive           Print type errors but compile anyway (not recommended)\n"
              << "  --freestanding         Emit a no-libc/no-GC object for kernel/bare-metal\n"
              << "  --no-gc                Do not link the garbage collector (alloc -> malloc)\n"
              << "  --no-ffi               Disable FFI support\n"
              << "  --no-concurrency       Disable concurrency features\n"
              << "  --no-advanced          Disable advanced language features\n"
              << "  --no-macros            Disable macro system\n"
              << "  --no-async             Disable async/await\n"
              << "  --debug                Enable debugger support\n"
              << "  --enable-python        Enable Python FFI (if available)\n"
              << "  --enable-javascript    Enable JavaScript FFI\n"
              << "  --enable-cpp           Enable C++ FFI\n"
              << std::endl;
    std::cout << "\nAdvanced Features:\n"
              << "  - Option/Result types for error handling\n"
              << "  - Traits and generics\n"
              << "  - Ownership and move semantics\n"
              << "  - Null safety\n"
              << "  - Concurrency with async/await\n"
              << "  - Macro system for compile-time code generation\n"
              << "  - FFI support (Python, JavaScript, C++)\n"
              << "  - LINQ-style data processing\n"
              << "  - Extension functions\n"
              << "  - WebAssembly target\n"
              << "  - Package manager\n"
              << "  - Debugger support\n"
              << std::endl;
}

/**
 * @brief Enhanced REPL with all new features
 */
void runEnhancedRepl(EnhancedCompiler &compiler, error::ErrorHandler &errorHandler)
{
    std::string line;
    EnhancedCompiler::CompilationOptions options;
    options.dumpIR = true;
    options.enableFFI = true;
    options.enableConcurrency = true;
    options.enableAdvancedFeatures = true;
    options.enableMacros = true;
    options.enableAsync = true;
    options.optimize = true;
    options.optimizationLevel = 2;

    // Initialize REPL state
    static int replCounter = 0;
    std::string replState;

    std::cout << "Tocin Enhanced REPL (type 'exit' to quit, 'clear' to reset)\n"
              << "Commands: debug, package, async, macro\n> ";

    while (std::getline(std::cin, line))
    {
        if (line == "exit" || line == "quit")
            break;
        if (line == "clear")
        {
            errorHandler.clearErrors();
            replState.clear();
            replCounter = 0;
            std::cout << "> ";
            continue;
        }

        // Handle special commands
        if (line == "debug") {
            std::cout << "Debugger commands: break, step, continue, variables, stack\n";
            std::cout << "> ";
            continue;
        }
        if (line == "package") {
            std::cout << "Package commands: install, uninstall, search, list\n";
            std::cout << "> ";
            continue;
        }
        if (line == "async") {
            std::cout << "Async commands: await, future, promise\n";
            std::cout << "> ";
            continue;
        }
        if (line == "macro") {
            std::cout << "Macro commands: define, expand, list\n";
            std::cout << "> ";
            continue;
        }

        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        
        if (line.empty())
        {
            std::cout << "> ";
            continue;
        }

        // Create a proper module for each REPL input
        std::string moduleSource;
        
        // Handle variable declarations and expressions
        if (line.find("let ") == 0 || line.find("const ") == 0)
        {
            // Variable declaration
            if (line.back() != ';')
                line += ";";
            moduleSource = replState + "\n" + line;
        }
        else if (line.find("def ") == 0 || line.find("class ") == 0 || 
                 line.find("trait ") == 0 || line.find("import ") == 0)
        {
            // Definition or import statement
            moduleSource = replState + "\n" + line;
        }
        else
        {
            // Expression - wrap in a main function if needed
            if (line.back() != ';')
                line += ";";
            
            // Create a temporary function to evaluate the expression
            std::string funcName = "repl_expr_" + std::to_string(replCounter++);
            moduleSource = replState + "\n" +
                         "def " + funcName + "() {\n" +
                         "    " + line + "\n" +
                         "}\n" +
                         funcName + "();";
        }

        // Compile the current state
        if (compiler.compile(moduleSource, "<repl>", options))
        {
            // Update REPL state with successful compilation
            replState = moduleSource;
        }
        else
        {
            // Clear errors but keep the previous state
            errorHandler.clearErrors();
        }

        std::cout << "> ";
    }
}

/**
 * @brief Main entry point for the enhanced Tocin compiler.
 */
int main(int argc, char *argv[])
{
    // Initialize LLVM
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

// Initialize Python if available
#ifdef WITH_PYTHON
    Py_Initialize();
#endif

    // Create error handler
    error::ErrorHandler errorHandler;

    // Create enhanced compiler
    EnhancedCompiler compiler(errorHandler);

    // If no arguments, run enhanced REPL
    if (argc == 1)
    {
        runEnhancedRepl(compiler, errorHandler);
        return 0;
    }

    // Parse command-line arguments
    EnhancedCompiler::CompilationOptions options;
    std::string filename;

    // Subcommands (cargo/go-style). `tocin check f.to` = typecheck only;
    // `tocin new name` scaffolds a project directory.
    int argStart = 1;
    bool checkOnly = false;
    if (argc >= 2 && std::string(argv[1]) == "check")
    {
        checkOnly = true;
        argStart = 2;
    }
    else if (argc >= 3 && std::string(argv[1]) == "doc")
    {
        // `tocin doc file.to` — generate Markdown API docs from top-level
        // signatures plus the contiguous `//` comment block above each one.
        std::string docFile = argv[2];
        std::ifstream in(docFile);
        if (!in)
        {
            std::cerr << "error: cannot open '" << docFile << "'" << std::endl;
            return 1;
        }
        std::string src((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        std::vector<std::string> lines;
        {
            std::stringstream ss(src);
            std::string l;
            while (std::getline(ss, l))
                lines.push_back(l);
        }
        lexer::Lexer lx(src, docFile, 4);
        auto toks = lx.tokenize();
        parser::Parser ps(toks);
        auto program = ps.parse();
        if (!program || errorHandler.hasFatalErrors())
        {
            std::cerr << "error: could not parse '" << docFile << "'" << std::endl;
            return 1;
        }
        // The `//` block immediately above `line` (1-based), joined as prose.
        auto docAbove = [&](int line) -> std::string {
            std::string out;
            for (int i = line - 2; i >= 0 && (size_t)i < lines.size(); --i)
            {
                std::string t = lines[i];
                size_t p = t.find_first_not_of(" \t");
                if (p == std::string::npos || t.compare(p, 2, "//") != 0)
                    break;
                std::string body = t.substr(p + 2);
                if (!body.empty() && body[0] == ' ')
                    body.erase(0, 1);
                out = body + (out.empty() ? "" : "\n") + out;
            }
            return out;
        };
        auto signatureOf = [](ast::FunctionStmt *fn) {
            std::string s = "def " + fn->name + "(";
            for (size_t i = 0; i < fn->parameters.size(); ++i)
            {
                if (i) s += ", ";
                s += fn->parameters[i].name;
                if (fn->parameters[i].type)
                    s += ": " + fn->parameters[i].type->toString();
            }
            s += ")";
            if (fn->returnType && fn->returnType->toString() != "None")
                s += " -> " + fn->returnType->toString();
            return s;
        };
        std::cout << "# " << docFile << "\n\n";
        std::vector<ast::StmtPtr> top;
        if (auto blk = std::dynamic_pointer_cast<ast::BlockStmt>(program)) top = blk->statements;
        else top.push_back(program);
        for (auto &s : top)
        {
            if (auto fn = std::dynamic_pointer_cast<ast::FunctionStmt>(s))
            {
                if (fn->name.rfind("__", 0) == 0)
                    continue;   // internal helpers stay out of the docs
                std::cout << "### `" << signatureOf(fn.get()) << "`\n\n";
                std::string d = docAbove(fn->token.line);
                if (!d.empty()) std::cout << d << "\n\n";
            }
            else if (auto cls = std::dynamic_pointer_cast<ast::ClassStmt>(s))
            {
                std::cout << "## class `" << cls->name << "`\n\n";
                std::string d = docAbove(cls->token.line);
                if (!d.empty()) std::cout << d << "\n\n";
                for (auto &m : cls->methods)
                    if (auto mf = std::dynamic_pointer_cast<ast::FunctionStmt>(m))
                        std::cout << "- `" << signatureOf(mf.get()) << "`\n";
                std::cout << "\n";
            }
            else if (auto en = std::dynamic_pointer_cast<ast::EnumStmt>(s))
            {
                std::cout << "## enum `" << en->name << "`\n\n";
                for (auto &m : en->members)
                    std::cout << "- `" << m.first << "`\n";
                std::cout << "\n";
            }
            else if (auto var = std::dynamic_pointer_cast<ast::VariableStmt>(s))
            {
                if (var->isConstant)
                    std::cout << "- const `" << var->name
                              << (var->type ? ": " + var->type->toString() : "") << "`\n";
            }
        }
        return 0;
    }
    else if (argc >= 3 && std::string(argv[1]) == "new")
    {
        namespace fs = std::filesystem;
        std::string name = argv[2];
        std::error_code ec;
        if (fs::exists(name, ec))
        {
            std::cerr << "error: '" << name << "' already exists" << std::endl;
            return 1;
        }
        fs::create_directories(name, ec);
        if (ec)
        {
            std::cerr << "error: cannot create directory '" << name << "': " << ec.message() << std::endl;
            return 1;
        }
        std::ofstream mainTo(fs::path(name) / "main.to");
        mainTo << "// " << name << " — created by `tocin new`\n"
               << "\n"
               << "def main() -> int {\n"
               << "    println(\"Hello from " << name << "!\");\n"
               << "    return 0;\n"
               << "}\n";
        std::ofstream readme(fs::path(name) / "README.md");
        readme << "# " << name << "\n\n"
               << "Run it:\n\n"
               << "```\n"
               << "tocin main.to --run          # JIT\n"
               << "tocin main.to -O3 -o " << name << "   # native binary\n"
               << "tocin check main.to          # typecheck only\n"
               << "```\n";
        std::ofstream gitignore(fs::path(name) / ".gitignore");
        gitignore << "/" << name << "\n*.o\n*.ll\n*.s\n";
        std::cout << "Created " << name << "/ (main.to, README.md, .gitignore)\n"
                  << "  cd " << name << " && tocin main.to --run" << std::endl;
        return 0;
    }

    for (int i = argStart; i < argc; ++i)
    {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-help" || arg == "-h" || arg == "/?")
        {
            displayUsage();
            return 0;
        }
        else if (arg == "--version" || arg == "-V")
        {
#ifndef TOCIN_VERSION
#define TOCIN_VERSION "0.0.0-dev"
#endif
            std::cout << "tocin " << TOCIN_VERSION << std::endl;
            return 0;
        }
        else if (arg == "--dump-ir")
        {
            options.dumpIR = true;
        }
        else if (arg == "--jit" || arg == "--run")
        {
            options.run = true;
        }
        else if (arg == "-O0")
        {
            options.optimize = true;
            options.optimizationLevel = 0;
        }
        else if (arg == "-O1")
        {
            options.optimize = true;
            options.optimizationLevel = 1;
        }
        else if (arg == "-O2")
        {
            options.optimize = true;
            options.optimizationLevel = 2;
        }
        else if (arg == "-O3")
        {
            options.optimize = true;
            options.optimizationLevel = 3;
        }
        else if (arg == "-o" && i + 1 < argc)
        {
            options.outputFile = argv[++i];
        }
        else if (arg == "--target" && i + 1 < argc)
        {
            options.target = argv[++i];
        }
        else if (arg == "--no-ffi")
        {
            options.enableFFI = false;
        }
        else if (arg == "--no-gc")
        {
            options.noGC = true;
        }
        else if (arg == "--borrow-check")
        {
            options.borrowCheck = true;
        }
        else if (arg == "--native" || arg == "-march=native" || arg == "-mcpu=native")
        {
            // Tune AOT codegen for the build host (POPCNT/AVX/BMI/...). Faster
            // but the resulting binary may not run on older CPUs.
            options.nativeCpu = true;
        }
        else if (arg == "--permissive")
        {
            // Lenient mode: type errors are printed but do not block codegen
            // (the pre-strict behavior). Fatal errors still stop compilation.
            options.permissive = true;
        }
        else if (arg == "--freestanding")
        {
            // Kernel / bare-metal: no libc, no GC, no runtime, no host concurrency.
            options.freestanding = true;
            options.noGC = true;
            options.enableFFI = false;
            options.enableConcurrency = false;
            options.enableAsync = false;
        }
        else if (arg == "--no-concurrency")
        {
            options.enableConcurrency = false;
        }
        else if (arg == "--no-advanced")
        {
            options.enableAdvancedFeatures = false;
        }
        else if (arg == "--no-macros")
        {
            options.enableMacros = false;
        }
        else if (arg == "--no-async")
        {
            options.enableAsync = false;
        }
        else if (arg == "--debug")
        {
            options.enableDebugger = true;
        }
        else if (arg == "--enable-python")
        {
            options.enableFFI = true;
        }
        else if (arg == "--enable-javascript")
        {
            options.enableFFI = true;
        }
        else if (arg == "--enable-cpp")
        {
            options.enableFFI = true;
        }
        else if (arg[0] == '-')
        {
            std::cerr << "Unknown option: " << arg << std::endl;
            displayUsage();
            return 1;
        }
        else
        {
            filename = arg;
        }
    }

    // Check if a filename was provided
    if (filename.empty())
    {
        std::cerr << "Error: No input file specified.\n";
        displayUsage();
        return 1;
    }

    // Read the source file
    std::ifstream file(filename);
    if (!file.is_open())
    {
        errorHandler.reportError(error::ErrorCode::I001_FILE_NOT_FOUND,
                                 "Could not open file: " + filename);
        return 1;
    }

    std::string source((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
    file.close();

    options.checkOnly = checkOnly;

    // Compile the source
    if (!compiler.compile(source, filename, options))
    {
        // Summary line so the tally is visible after a wall of diagnostics.
        int ec = errorHandler.errorCount();
        int wc = errorHandler.warningCount();
        std::cerr << (ec == 1 ? "1 error" : std::to_string(ec) + " errors");
        if (wc > 0)
            std::cerr << ", " << (wc == 1 ? "1 warning" : std::to_string(wc) + " warnings");
        std::cerr << " generated." << std::endl;
        return 1;
    }
    if (checkOnly)
    {
        int wc = errorHandler.warningCount();
        std::cout << filename << ": OK"
                  << (wc > 0 ? " (" + std::to_string(wc) + " warning" + (wc == 1 ? "" : "s") + ")" : "")
                  << std::endl;
        return 0;
    }

// Clean up Python if it was initialized
#ifdef WITH_PYTHON
    Py_Finalize();
#endif

    // When JIT-executing, propagate the program's exit code.
    if (options.run)
    {
        return compiler.getProgramExitCode();
    }

    return 0;
}
