#include "error_handling.h"

namespace tocin {
namespace util {

// ErrorInfo implementation
ErrorInfo::ErrorInfo(ErrorCode code, const std::string& message, const std::string& location)
    : code_(code), message_(message), location_(location) {}

ErrorCode ErrorInfo::code() const {
    return code_;
}

std::string ErrorInfo::message() const {
    return message_;
}

std::string ErrorInfo::location() const {
    return location_;
}

std::string ErrorInfo::to_string() const {
    std::string result = "[";
    switch (code_) {
        case ErrorCode::Success: result += "SUCCESS"; break;
        case ErrorCode::SyntaxError: result += "SYNTAX_ERROR"; break;
        case ErrorCode::TypeError: result += "TYPE_ERROR"; break;
        case ErrorCode::RuntimeError: result += "RUNTIME_ERROR"; break;
        case ErrorCode::IOError: result += "IO_ERROR"; break;
        case ErrorCode::MemoryError: result += "MEMORY_ERROR"; break;
        case ErrorCode::CompilationError: result += "COMPILATION_ERROR"; break;
        case ErrorCode::LinkError: result += "LINK_ERROR"; break;
        case ErrorCode::InternalError: result += "INTERNAL_ERROR"; break;
        default: result += "UNKNOWN"; break;
    }
    result += "] ";
    if (!location_.empty()) {
        result += location_ + ": ";
    }
    result += message_;
    return result;
}

} // namespace util
} // namespace tocin
