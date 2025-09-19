#ifndef ERROR_HANDLER_H
#define ERROR_HANDLER_H

#include <string>
#include <vector>
#include <mutex>
#include <variant> // For optional data
#include "../lexer/token.h" // Include token.h for Token definition

namespace error {

    // Define specific error codes
    enum class ErrorCode {
        // Lexical Errors (L001-L099)
        L001_INVALID_CHARACTER,
        L002_UNTERMINATED_STRING,
        L003_INVALID_NUMBER_FORMAT,

        // Syntax Errors (S001-S099)
        S001_UNEXPECTED_TOKEN,
        S002_MISSING_EXPECTED_TOKEN, // e.g., missing semicolon
        S003_INVALID_EXPRESSION,
        S004_INVALID_STATEMENT,
        S005_INVALID_ASSIGNMENT_TARGET,

        // Type Errors (T001-T099)
        T001_TYPE_MISMATCH,
        T002_UNDEFINED_VARIABLE,
        T003_UNDEFINED_FUNCTION,
        T004_UNDEFINED_TYPE,
        T005_UNDEFINED_MEMBER, // For classes/structs
        T006_INVALID_OPERATOR_FOR_TYPE,
        T007_INCORRECT_ARGUMENT_COUNT,
        T008_INCORRECT_ARGUMENT_TYPE,
        T009_CANNOT_INFER_TYPE,
        T010_RETURN_TYPE_MISMATCH,

        // Semantic Errors (M001-M099) - More general meaning errors
        M001_DUPLICATE_DEFINITION,
        M002_INVALID_BREAK_CONTINUE, // Outside loop
        M003_INVALID_RETURN,         // Outside function
        M004_UNREACHABLE_CODE,       // Potentially a warning
        M005_UNUSED_VARIABLE,        // Potentially a warning
        M006_MODULE_NOT_FOUND,
        M007_CIRCULAR_DEPENDENCY,

        // FFI Errors (F001-F099)
        F001_FFI_CALL_FAILED,
        F002_FFI_TYPE_CONVERSION_ERROR,
        F003_FFI_SETUP_ERROR,

        // I/O Errors (I001-I099)
        I001_FILE_NOT_FOUND,
        I002_PERMISSION_DENIED,
        I003_READ_ERROR,
        I004_WRITE_ERROR,

        // Internal Compiler Errors (C001-C099) - Bugs in the compiler itself
        C001_UNIMPLEMENTED_FEATURE,
        C002_CODEGEN_ERROR,
        C003_TYPECHECK_ERROR, // Internal logic error
        C004_INTERNAL_ASSERTION_FAILED,

        // Generic/Unknown
        G001_UNKNOWN_ERROR,
        G002_GENERAL_SYNTAX_ERROR,
        G003_GENERAL_TYPE_ERROR,
        G004_GENERAL_SEMANTIC_ERROR,
    };

    // Function to convert ErrorCode to string (optional but helpful)
    std::string errorCodeToString(ErrorCode code);

    enum class ErrorSeverity { WARNING, ERROR, FATAL };

    struct Error {
        ErrorCode code; // Added ErrorCode
        std::string message;
        std::string filename;
        int line;
        int column;
        ErrorSeverity severity;
        // Optional: std::variant<...> contextData; // Could hold expected token, type names etc.

        Error(ErrorCode errCode, std::string msg, std::string file, int l, int c, ErrorSeverity sev)
            : code(errCode), message(std::move(msg)), filename(std::move(file)), line(l), column(c), severity(sev) {}
    };

    class ErrorHandler {
    public:
        ErrorHandler() = default;
        explicit ErrorHandler(const std::string& filename) : defaultFilename(filename) {}

        // Updated reportError methods
        void reportError(ErrorCode code, const std::string& message, const std::string& filename, int line, int column,
                         ErrorSeverity severity = ErrorSeverity::ERROR);

        void reportError(ErrorCode code, const std::string& message, const lexer::Token& token,
                         ErrorSeverity severity = ErrorSeverity::ERROR);

        // For general errors without specific location info (use sparingly)
        void reportError(ErrorCode code, const std::string& message,
                         ErrorSeverity severity = ErrorSeverity::ERROR);


        const std::vector<Error>& getErrors() const;
        bool hasErrors() const;
        bool hasFatalErrors() const;
        void setFatal(bool fatal); // Maybe remove if FATAL severity is used consistently
        bool isFatal() const;    // Maybe remove if FATAL severity is used consistently
        void clearErrors();      // Useful for REPL or interactive modes


    private:
        std::vector<Error> errors;
        mutable std::mutex mutex;
        bool fatalErrorFound = false; // Consider relying solely on severity in the errors vector
        std::string defaultFilename = "<unknown>";
    };

} // namespace error

#endif // ERROR_HANDLER_H
