/**
 * Target Information Helper
 *
 * This header provides functions to detect system information
 * when LLVM's Host.h is not available.
 */

#ifndef TOCIN_TARGET_INFO_H
#define TOCIN_TARGET_INFO_H

#include <string>
#include <fstream>
#include <algorithm>

namespace tocin {
namespace system {

/**
 * @brief Detect CPU architecture.
 * @return String describing the CPU architecture.
 */
inline std::string detectArchitecture() {
#if defined(__x86_64__) || defined(_M_X64)
    return "x86_64";
#elif defined(i386) || defined(__i386__) || defined(__i386) || defined(_M_IX86)
    return "x86";
#elif defined(__aarch64__) || defined(_M_ARM64)
    return "aarch64";
#elif defined(__arm__) || defined(_M_ARM)
    return "arm";
#else
    return "unknown";
#endif
}

/**
 * @brief Detect operating system.
 * @return String describing the operating system.
 */
inline std::string detectOS() {
#if defined(_WIN32) || defined(_WIN64)
    return "windows";
#elif defined(__APPLE__)
    return "darwin";
#elif defined(__linux__)
    return "linux";
#elif defined(__FreeBSD__)
    return "freebsd";
#else
    return "unknown";
#endif
}

/**
 * @brief Detect compiler/environment.
 * @return String describing the compiler environment.
 */
inline std::string detectEnvironment() {
#if defined(_MSC_VER)
    return "msvc";
#elif defined(__MINGW32__) || defined(__MINGW64__)
    return "gnu";
#elif defined(__GNUC__)
    return "gnu";
#elif defined(__clang__)
    return "llvm";
#else
    return "unknown";
#endif
}

/**
 * @brief Generate a target triple in LLVM format.
 * @return String representing the target triple.
 */
inline std::string getTargetTriple() {
    std::string arch = detectArchitecture();
    std::string vendor = "pc"; // Default to PC for most systems
    std::string os = detectOS();
    std::string env = detectEnvironment();
    
    // Special handling for Apple platforms
    if (os == "darwin") {
        vendor = "apple";
    }
    
    return arch + "-" + vendor + "-" + os + "-" + env;
}

/**
 * @brief Detect CPU model and features.
 * @return String identifying the CPU.
 */
inline std::string getCPUName() {
    std::string cpu = "generic";
    
#if defined(_WIN32) || defined(_WIN64)
    // Try to get CPU model from registry or WMI
    // Simplified to return a generic value for now
    cpu = "x86-64";
#elif defined(__linux__)
    // Try to read from /proc/cpuinfo
    std::ifstream cpuinfo("/proc/cpuinfo");
    if (cpuinfo.is_open()) {
        std::string line;
        while (std::getline(cpuinfo, line)) {
            if (line.find("model name") != std::string::npos) {
                size_t pos = line.find(':');
                if (pos != std::string::npos) {
                    cpu = line.substr(pos + 2);
                    break;
                }
            }
        }
    }
#endif

    return cpu;
}

} // namespace system
} // namespace tocin

#endif // TOCIN_TARGET_INFO_H 
