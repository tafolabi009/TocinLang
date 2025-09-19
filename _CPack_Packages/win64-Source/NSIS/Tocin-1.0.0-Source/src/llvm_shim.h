#ifndef LLVM_SHIM_H
#define LLVM_SHIM_H

// LLVM shim header to handle different LLVM versions and include paths
#if __has_include(<llvm/Support/Host.h>)
#include <llvm/Support/Host.h>
#define LLVM_HOST_HEADER_AVAILABLE 1
#else
#define LLVM_HOST_HEADER_AVAILABLE 0
#endif
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/FileSystem.h>

// If Host.h is not available, provide minimal fallbacks
#if !LLVM_HOST_HEADER_AVAILABLE
namespace llvm {
namespace sys {
    inline std::string getDefaultTargetTriple() { return "x86_64-unknown-linux-gnu"; }
    inline std::string getProcessTriple() { return getDefaultTargetTriple(); }
    inline std::string getHostCPUName() { return "generic"; }
    inline std::string getHostCPUFeatures() { return ""; }
} // namespace sys
} // namespace llvm
#endif

// Try to include CPU info headers
#if __has_include(<llvm/Support/CPU.h>)
#include <llvm/Support/CPU.h>
#define LLVM_CPU_HEADER_AVAILABLE 1
#else
#define LLVM_CPU_HEADER_AVAILABLE 0

#if !LLVM_HOST_HEADER_AVAILABLE
namespace llvm
{
    namespace sys
    {
        // Additional CPU-related functions if needed
    }
}
#endif
#endif

// Try to include Error Handling headers
#if __has_include(<llvm/Support/ErrorHandling.h>)
#include <llvm/Support/ErrorHandling.h>
#define LLVM_ERROR_HANDLING_AVAILABLE 1
#else
#define LLVM_ERROR_HANDLING_AVAILABLE 0

namespace llvm
{
    // Very minimal error handling fallbacks if needed
    inline void report_fatal_error(const std::string &reason, bool gen_crash_diag = true)
    {
        fprintf(stderr, "LLVM ERROR: %s\n", reason.c_str());
        abort();
    }
}
#endif

// Add CodeGenFileType constants that might be missing in some LLVM versions
#if !defined(LLVM_CGFT_DEFINED) && !__has_include(<llvm/Support/CodeGen.h>)
namespace llvm
{
    enum class CodeGenFileType
    {
        CGFT_AssemblyFile,
        CGFT_ObjectFile,
        CGFT_Null
    };
}
#define LLVM_CGFT_DEFINED
#endif

// Add other LLVM shims as needed here

#endif // TOCIN_LLVM_SHIM_H
