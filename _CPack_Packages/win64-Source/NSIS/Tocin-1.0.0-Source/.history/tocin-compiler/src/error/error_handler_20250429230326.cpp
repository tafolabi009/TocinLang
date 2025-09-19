#include "error_handler.h"
#include <iostream>
#include <sstream> // For errorCodeToString

namespace error {

    // Basic implementation of errorCodeToString
    // (Can be made more sophisticated, e.g., using a map or switch)
    std::string errorCodeToString(ErrorCode code) {
       switch(code) {
           // Add cases for all your error codes...
           case ErrorCode::L001_INVALID_CHARACTER: return "L001";
           case ErrorCode::L002_UNTERMINATED_STRING: return "L002";
           case ErrorCode::S001_UNEXPECTED_TOKEN: return "S001";
           case ErrorCode::S002_MISSING_EXPECTED_TOKEN: return "S002";
           case ErrorCode::T001_TYPE_MISMATCH: return "T001";
           case ErrorCode::T002_UNDEFINED_VARIABLE: return "T002";
           case ErrorCode::M001_DUPLICATE_DEFINITION: return "M001";
           case ErrorCode::M006_MODULE_NOT_FOUND: return "M006";
           case ErrorCode::F001_FFI_CALL_FAILED: return "F001";
           case ErrorCode::I001_FILE_NOT_FOUND: return "I001";
           case ErrorCode::C001_UNIMPLEMENTED_FEATURE: return "C001";
           case ErrorCode::C004_INTERNAL_ASSERTION_FAILED: return "C004";
           // ... other cases ...
           default: return "G001"; // Generic/Unknown
       }
    }

    // Updated reportError using Token
    void ErrorHandler::reportError(ErrorCode code, const std::string& message, const lexer::Token& token, ErrorSeverity severity) {
        reportError(code, message, token.filename, token.line, token.column, severity);
    }

    // Updated primary reportError method
    void ErrorHandler::reportError(ErrorCode code, const std::string& message, const std::string& filename,
                                   int line, int column, ErrorSeverity severity) {
        std::lock_guard<std::mutex> lock(mutex);
        std::string effectiveFilename = filename.empty() ? defaultFilename : filename;
        errors.emplace_back(code, message, effectiveFilename, line, column, severity);

        if (severity == ErrorSeverity::FATAL) {
            fatalErrorFound = true;
        }

        // Improved output format
        std::cerr << effectiveFilename << ":" << line << ":" << column << ": "
                  << (severity == ErrorSeverity::WARNING ? "warning" : "error") // Keep 'error' for ERROR and FATAL for simplicity here
                  << " [" << errorCodeToString(code) << "]: " // Include error code
                  << message << std::endl;

         // Optionally: Print code snippet where error occurred if source lines are available
    }

    // Updated reportError for general errors
    void ErrorHandler::reportError(ErrorCode code, const std::string& message, ErrorSeverity severity) {
        // Report with unknown location, maybe line 0 or -1
        reportError(code, message, defaultFilename, 0, 0, severity);
    }

    bool ErrorHandler::hasErrors() const {
        std::lock_guard<std::mutex> lock(mutex);
        // Check if any error has severity ERROR or FATAL
        for (const auto& err : errors) {
            if (err.severity == ErrorSeverity::ERROR || err.severity == ErrorSeverity::FATAL) {
                return true;
            }
        }
        return false;
    }

    bool ErrorHandler::hasFatalErrors() const {
        std::lock_guard<std::mutex> lock(mutex);
        // Check if any error has severity FATAL
         for (const auto& err : errors) {
            if (err.severity == ErrorSeverity::FATAL) {
                return true; // Found a fatal error
            }
        }
        // Or return the old flag if you still use setFatal explicitly elsewhere
        // return fatalErrorFound;
        return false; // No fatal severity errors found
    }

    const std::vector<Error>& ErrorHandler::getErrors() const {
        // No lock needed if returning const& to immutable data, but depends on usage.
        // Keeping lock for consistency with original code.
        std::lock_guard<std::mutex> lock(mutex);
        return errors;
    }

    // You might remove setFatal and isFatal if you rely solely on the ErrorSeverity::FATAL
    // in the errors vector. Keeping them for now if other logic uses them.
    void ErrorHandler::setFatal(bool fatal) {
        std::lock_guard<std::mutex> lock(mutex);
        fatalErrorFound = fatal;
        // Optionally, you could also add a generic FATAL error to the list here
    }

    bool ErrorHandler::isFatal() const {
        std::lock_guard<std::mutex> lock(mutex);
        return fatalErrorFound || hasFatalErrors(); // Check both flag and list
    }

    void ErrorHandler::clearErrors() {
         std::lock_guard<std::mutex> lock(mutex);
         errors.clear();
         fatalErrorFound = false; // Reset fatal flag too
    }


} // namespace error
