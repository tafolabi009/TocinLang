/**
 * @file memory_safe.cpp
 * @brief Implementation of memory-safe RAII wrappers
 */

#include "memory_safe.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace tocin {
namespace util {

LibraryHandle::~LibraryHandle() {
    close();
}

void LibraryHandle::close() {
    if (handle_) {
#ifdef _WIN32
        FreeLibrary(static_cast<HMODULE>(handle_));
#else
        dlclose(handle_);
#endif
        handle_ = nullptr;
    }
}

void* LibraryHandle::getSymbol(const char* name) const {
    if (!handle_) {
        return nullptr;
    }
    
#ifdef _WIN32
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle_), name));
#else
    return dlsym(handle_, name);
#endif
}

} // namespace util
} // namespace tocin
