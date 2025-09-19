#include "error_handler.h"
#include <iostream>
#include <sstream> // For errorCodeToString

namespace error
{

    // Basic implementation of errorCodeToString
    // (Can be made more sophisticated, e.g., using a map or switch)
    std::string errorCodeToString(ErrorCode code)
    {
        switch (code)
        {
        // Lexical Errors
        case ErrorCode::L001_INVALID_CHARACTER: return "L001";
        case ErrorCode::L002_UNTERMINATED_STRING: return "L002";
        case ErrorCode::L003_INVALID_NUMBER_FORMAT: return "L003";
        case ErrorCode::L004_TOO_MANY_ERRORS: return "L004";
        case ErrorCode::L005_INVALID_ESCAPE_SEQUENCE: return "L005";
        case ErrorCode::L006_INVALID_UNICODE_ESCAPE: return "L006";
        case ErrorCode::L007_INVALID_TEMPLATE_LITERAL: return "L007";

        // Syntax Errors
        case ErrorCode::S001_UNEXPECTED_TOKEN: return "S001";
        case ErrorCode::S002_MISSING_EXPECTED_TOKEN: return "S002";
        case ErrorCode::S003_INVALID_EXPRESSION: return "S003";
        case ErrorCode::S004_INVALID_STATEMENT: return "S004";
        case ErrorCode::S005_INVALID_ASSIGNMENT_TARGET: return "S005";
        case ErrorCode::S006_INVALID_FUNCTION_DECLARATION: return "S006";
        case ErrorCode::S007_INVALID_CLASS_DECLARATION: return "S007";
        case ErrorCode::S008_INVALID_IMPORT_STATEMENT: return "S008";
        case ErrorCode::S009_INVALID_MATCH_STATEMENT: return "S009";
        case ErrorCode::S010_INVALID_TRY_CATCH_BLOCK: return "S010";
        case ErrorCode::S011_INVALID_LOOP_STATEMENT: return "S011";
        case ErrorCode::S012_INVALID_SWITCH_STATEMENT: return "S012";
        case ErrorCode::S013_INVALID_ENUM_DECLARATION: return "S013";
        case ErrorCode::S014_INVALID_STRUCT_DECLARATION: return "S014";
        case ErrorCode::S015_INVALID_INTERFACE_DECLARATION: return "S015";
        case ErrorCode::S016_INVALID_TRAIT_DECLARATION: return "S016";
        case ErrorCode::S017_INVALID_IMPL_BLOCK: return "S017";
        case ErrorCode::S018_INVALID_MODULE_DECLARATION: return "S018";
        case ErrorCode::S019_INVALID_NAMESPACE_DECLARATION: return "S019";
        case ErrorCode::S020_INVALID_DEFER_STATEMENT: return "S020";

        // Type Errors
        case ErrorCode::T001_TYPE_MISMATCH: return "T001";
        case ErrorCode::T002_UNDEFINED_VARIABLE: return "T002";
        case ErrorCode::T003_UNDEFINED_FUNCTION: return "T003";
        case ErrorCode::T004_UNDEFINED_CLASS: return "T004";
        case ErrorCode::T005_UNDEFINED_METHOD: return "T005";
        case ErrorCode::T006_INVALID_OPERATOR_FOR_TYPE: return "T006";
        case ErrorCode::T007_INVALID_FUNCTION_CALL: return "T007";
        case ErrorCode::T008_INVALID_METHOD_CALL: return "T008";
        case ErrorCode::T009_INVALID_CONSTRUCTOR_CALL: return "T009";
        case ErrorCode::T010_INVALID_DESTRUCTOR_CALL: return "T010";
        case ErrorCode::T011_INVALID_CAST: return "T011";
        case ErrorCode::T012_INVALID_CONVERSION: return "T012";
        case ErrorCode::T013_INVALID_ASSIGNMENT: return "T013";
        case ErrorCode::T014_INVALID_RETURN_TYPE: return "T014";
        case ErrorCode::T015_INVALID_PARAMETER_TYPE: return "T015";
        case ErrorCode::T016_INVALID_GENERIC_TYPE: return "T016";
        case ErrorCode::T017_INVALID_TRAIT_IMPLEMENTATION: return "T017";
        case ErrorCode::T018_INVALID_INTERFACE_IMPLEMENTATION: return "T018";
        case ErrorCode::T019_INVALID_INHERITANCE: return "T019";
        case ErrorCode::T020_INVALID_OVERRIDE: return "T020";
        case ErrorCode::T021_INVALID_ABSTRACT_METHOD: return "T021";
        case ErrorCode::T022_INVALID_FINAL_OVERRIDE: return "T022";
        case ErrorCode::T023_INVALID_STATIC_METHOD: return "T023";
        case ErrorCode::T024_INVALID_VIRTUAL_METHOD: return "T024";
        case ErrorCode::T025_INVALID_CONST_METHOD: return "T025";
        case ErrorCode::T026_INVALID_MUTABLE_REFERENCE: return "T026";
        case ErrorCode::T027_INVALID_IMMUTABLE_REFERENCE: return "T027";
        case ErrorCode::T028_INVALID_MOVE_SEMANTICS: return "T028";
        case ErrorCode::T029_INVALID_OWNERSHIP_TRANSFER: return "T029";
        case ErrorCode::T030_INVALID_BORROW_CHECK: return "T030";

        // Module Errors
        case ErrorCode::M001_DUPLICATE_DEFINITION: return "M001";
        case ErrorCode::M002_CIRCULAR_DEPENDENCY: return "M002";
        case ErrorCode::M003_INVALID_MODULE_PATH: return "M003";
        case ErrorCode::M004_MODULE_NOT_FOUND: return "M004";
        case ErrorCode::M005_INVALID_MODULE_FORMAT: return "M005";
        case ErrorCode::M006_MODULE_NOT_FOUND: return "M006";
        case ErrorCode::M007_INVALID_PACKAGE_NAME: return "M007";
        case ErrorCode::M008_INVALID_NAMESPACE_NAME: return "M008";
        case ErrorCode::M009_INVALID_IMPORT_PATH: return "M009";
        case ErrorCode::M010_INVALID_EXPORT_STATEMENT: return "M010";
        case ErrorCode::M011_INVALID_MODULE_STRUCTURE: return "M011";
        case ErrorCode::M012_INVALID_PACKAGE_STRUCTURE: return "M012";
        case ErrorCode::M013_INVALID_NAMESPACE_STRUCTURE: return "M013";
        case ErrorCode::M014_INVALID_DEPENDENCY_DECLARATION: return "M014";
        case ErrorCode::M015_INVALID_VERSION_SPECIFICATION: return "M015";

        // FFI Errors
        case ErrorCode::F001_FFI_CALL_FAILED: return "F001";
        case ErrorCode::F002_INVALID_FFI_SIGNATURE: return "F002";
        case ErrorCode::F003_INVALID_FFI_TYPE: return "F003";
        case ErrorCode::F004_INVALID_FFI_LIBRARY: return "F004";
        case ErrorCode::F005_INVALID_FFI_FUNCTION: return "F005";
        case ErrorCode::F006_INVALID_FFI_PARAMETER: return "F006";
        case ErrorCode::F007_INVALID_FFI_RETURN_TYPE: return "F007";
        case ErrorCode::F008_INVALID_FFI_CALLING_CONVENTION: return "F008";
        case ErrorCode::F009_INVALID_FFI_MARSHALING: return "F009";
        case ErrorCode::F010_INVALID_FFI_UNMARSHALING: return "F010";
        case ErrorCode::F011_INVALID_FFI_MEMORY_MANAGEMENT: return "F011";
        case ErrorCode::F012_INVALID_FFI_ERROR_HANDLING: return "F012";
        case ErrorCode::F013_INVALID_FFI_THREAD_SAFETY: return "F013";
        case ErrorCode::F014_INVALID_FFI_EXCEPTION_HANDLING: return "F014";
        case ErrorCode::F015_INVALID_FFI_RESOURCE_MANAGEMENT: return "F015";

        // Concurrency Errors
        case ErrorCode::C001_CONCURRENCY_ERROR: return "C001";
        case ErrorCode::C002_DEADLOCK_DETECTED: return "C002";
        case ErrorCode::C003_RACE_CONDITION_DETECTED: return "C003";
        case ErrorCode::C004_INVALID_THREAD_OPERATION: return "C004";
        case ErrorCode::C005_INVALID_MUTEX_OPERATION: return "C005";
        case ErrorCode::C006_INVALID_CONDITION_VARIABLE_OPERATION: return "C006";
        case ErrorCode::C007_INVALID_SEMAPHORE_OPERATION: return "C007";
        case ErrorCode::C008_INVALID_BARRIER_OPERATION: return "C008";
        case ErrorCode::C009_INVALID_FUTURE_OPERATION: return "C009";
        case ErrorCode::C010_INVALID_PROMISE_OPERATION: return "C010";
        case ErrorCode::C011_INVALID_CHANNEL_OPERATION: return "C011";
        case ErrorCode::C012_INVALID_SELECT_OPERATION: return "C012";
        case ErrorCode::C013_INVALID_SPAWN_OPERATION: return "C013";
        case ErrorCode::C014_INVALID_JOIN_OPERATION: return "C014";
        case ErrorCode::C015_INVALID_YIELD_OPERATION: return "C015";
        case ErrorCode::C016_INVALID_COROUTINE_OPERATION: return "C016";
        case ErrorCode::C017_INVALID_GENERATOR_OPERATION: return "C017";
        case ErrorCode::C018_INVALID_ASYNC_OPERATION: return "C018";
        case ErrorCode::C019_INVALID_AWAIT_OPERATION: return "C019";
        case ErrorCode::C020_INVALID_ATOMIC_OPERATION: return "C020";
        case ErrorCode::C021_INVALID_MEMORY_ORDERING: return "C021";
        case ErrorCode::C022_INVALID_FENCE_OPERATION: return "C022";
        case ErrorCode::C023_INVALID_COMPARE_EXCHANGE_OPERATION: return "C023";
        case ErrorCode::C024_INVALID_FETCH_OPERATION: return "C024";
        case ErrorCode::C025_INVALID_STORE_OPERATION: return "C025";
        case ErrorCode::C026_INVALID_LOAD_OPERATION: return "C026";
        case ErrorCode::C027_INVALID_EXCHANGE_OPERATION: return "C027";
        case ErrorCode::C028_INVALID_TEST_AND_SET_OPERATION: return "C028";
        case ErrorCode::C029_INVALID_CLEAR_OPERATION: return "C029";
        case ErrorCode::C030_INVALID_NOTIFY_OPERATION: return "C030";

        // Runtime Errors
        case ErrorCode::R001_RUNTIME_ERROR: return "R001";
        case ErrorCode::R002_NULL_POINTER_DEREFERENCE: return "R002";
        case ErrorCode::R003_DIVISION_BY_ZERO: return "R003";
        case ErrorCode::R004_ARRAY_INDEX_OUT_OF_BOUNDS: return "R004";
        case ErrorCode::R005_STACK_OVERFLOW: return "R005";
        case ErrorCode::R006_HEAP_OVERFLOW: return "R006";
        case ErrorCode::R007_MEMORY_LEAK_DETECTED: return "R007";
        case ErrorCode::R008_DOUBLE_FREE_DETECTED: return "R008";
        case ErrorCode::R009_USE_AFTER_FREE_DETECTED: return "R009";
        case ErrorCode::R010_BUFFER_OVERFLOW_DETECTED: return "R010";
        case ErrorCode::R011_INTEGER_OVERFLOW_DETECTED: return "R011";
        case ErrorCode::R012_FLOATING_POINT_EXCEPTION: return "R012";
        case ErrorCode::R013_INVALID_MEMORY_ACCESS: return "R013";
        case ErrorCode::R014_INVALID_MEMORY_ALIGNMENT: return "R014";
        case ErrorCode::R015_INVALID_MEMORY_SIZE: return "R015";
        case ErrorCode::R016_INVALID_MEMORY_ADDRESS: return "R016";
        case ErrorCode::R017_INVALID_MEMORY_MAPPING: return "R017";
        case ErrorCode::R018_INVALID_MEMORY_PROTECTION: return "R018";
        case ErrorCode::R019_INVALID_MEMORY_PERMISSION: return "R019";
        case ErrorCode::R020_INVALID_MEMORY_REGION: return "R020";
        case ErrorCode::R021_INVALID_MEMORY_OPERATION: return "R021";
        case ErrorCode::R022_INVALID_MEMORY_STATE: return "R022";
        case ErrorCode::R023_INVALID_MEMORY_CONSISTENCY: return "R023";
        case ErrorCode::R024_INVALID_MEMORY_ORDERING: return "R024";
        case ErrorCode::R025_INVALID_MEMORY_FENCE: return "R025";
        case ErrorCode::R026_INVALID_MEMORY_BARRIER: return "R026";
        case ErrorCode::R027_INVALID_MEMORY_SYNCHRONIZATION: return "R027";
        case ErrorCode::R028_INVALID_MEMORY_COHERENCE: return "R028";
        case ErrorCode::R029_INVALID_MEMORY_VISIBILITY: return "R029";
        case ErrorCode::R030_INVALID_MEMORY_PERSISTENCE: return "R030";

        // I/O Errors
        case ErrorCode::I001_FILE_NOT_FOUND: return "I001";
        case ErrorCode::I002_FILE_ACCESS_DENIED: return "I002";
        case ErrorCode::I003_READ_ERROR: return "I003";
        case ErrorCode::I004_WRITE_ERROR: return "I004";
        case ErrorCode::I005_FILE_ALREADY_EXISTS: return "I005";
        case ErrorCode::I006_INVALID_FILE_PATH: return "I006";
        case ErrorCode::I007_INVALID_FILE_FORMAT: return "I007";
        case ErrorCode::I008_INVALID_FILE_ENCODING: return "I008";
        case ErrorCode::I009_INVALID_FILE_PERMISSION: return "I009";
        case ErrorCode::I010_INVALID_FILE_MODE: return "I010";
        case ErrorCode::I011_INVALID_FILE_DESCRIPTOR: return "I011";
        case ErrorCode::I012_INVALID_FILE_OPERATION: return "I012";
        case ErrorCode::I013_INVALID_FILE_STATE: return "I013";
        case ErrorCode::I014_INVALID_FILE_POSITION: return "I014";
        case ErrorCode::I015_INVALID_FILE_SIZE: return "I015";
        case ErrorCode::I016_INVALID_FILE_TIMESTAMP: return "I016";
        case ErrorCode::I017_INVALID_FILE_ATTRIBUTE: return "I017";
        case ErrorCode::I018_INVALID_FILE_METADATA: return "I018";
        case ErrorCode::I019_INVALID_FILE_CONTENT: return "I019";
        case ErrorCode::I020_INVALID_FILE_STRUCTURE: return "I020";
        case ErrorCode::I021_INVALID_FILE_VERSION: return "I021";
        case ErrorCode::I022_INVALID_FILE_CHECKSUM: return "I022";
        case ErrorCode::I023_INVALID_FILE_SIGNATURE: return "I023";
        case ErrorCode::I024_INVALID_FILE_HEADER: return "I024";
        case ErrorCode::I025_INVALID_FILE_FOOTER: return "I025";
        case ErrorCode::I026_INVALID_FILE_SECTION: return "I026";
        case ErrorCode::I027_INVALID_FILE_SEGMENT: return "I027";
        case ErrorCode::I028_INVALID_FILE_BLOCK: return "I028";
        case ErrorCode::I029_INVALID_FILE_RECORD: return "I029";
        case ErrorCode::I030_INVALID_FILE_ENTRY: return "I030";

        // Compiler Errors
        case ErrorCode::C001_UNIMPLEMENTED_FEATURE: return "C001";
        case ErrorCode::C002_CODEGEN_ERROR: return "C002";
        case ErrorCode::C003_OPTIMIZATION_ERROR: return "C003";
        case ErrorCode::C004_INTERNAL_ASSERTION_FAILED: return "C004";
        case ErrorCode::C005_INVALID_IR_GENERATION: return "C005";
        case ErrorCode::C006_INVALID_OPTIMIZATION_PASS: return "C006";
        case ErrorCode::C007_INVALID_CODE_GENERATION: return "C007";
        case ErrorCode::C008_INVALID_LINKING: return "C008";
        case ErrorCode::C009_INVALID_ASSEMBLY_GENERATION: return "C009";
        case ErrorCode::C010_INVALID_OBJECT_FILE_GENERATION: return "C010";
        case ErrorCode::C011_INVALID_EXECUTABLE_GENERATION: return "C011";
        case ErrorCode::C012_INVALID_LIBRARY_GENERATION: return "C012";
        case ErrorCode::C013_INVALID_MODULE_GENERATION: return "C013";
        case ErrorCode::C014_INVALID_PACKAGE_GENERATION: return "C014";
        case ErrorCode::C015_INVALID_DISTRIBUTION_GENERATION: return "C015";
        case ErrorCode::C016_INVALID_INSTALLER_GENERATION: return "C016";
        case ErrorCode::C017_INVALID_DEPLOYMENT_GENERATION: return "C017";
        case ErrorCode::C018_INVALID_PACKAGING: return "C018";
        case ErrorCode::C019_INVALID_SIGNING: return "C019";
        case ErrorCode::C020_INVALID_VERIFICATION: return "C020";
        case ErrorCode::C021_INVALID_VALIDATION: return "C021";
        case ErrorCode::C022_INVALID_SANITIZATION: return "C022";
        case ErrorCode::C023_INVALID_TRANSFORMATION: return "C023";
        case ErrorCode::C024_INVALID_EMISSION: return "C024";
        case ErrorCode::C025_INVALID_SERIALIZATION: return "C025";
        case ErrorCode::C026_INVALID_DESERIALIZATION: return "C026";
        case ErrorCode::C027_INVALID_MARSHALING: return "C027";
        case ErrorCode::C028_INVALID_UNMARSHALING: return "C028";
        case ErrorCode::C029_INVALID_ENCODING: return "C029";
        case ErrorCode::C030_INVALID_DECODING: return "C030";
        case ErrorCode::C031_TYPECHECK_ERROR: return "C031";

        // Pattern Matching Errors
        case ErrorCode::P001_NON_EXHAUSTIVE_PATTERNS: return "P001";
        case ErrorCode::P002_INVALID_PATTERN: return "P002";
        case ErrorCode::P003_INVALID_PATTERN_BINDING: return "P003";
        case ErrorCode::P004_INVALID_PATTERN_GUARD: return "P004";
        case ErrorCode::P005_INVALID_PATTERN_TYPE: return "P005";

        // Borrowing/Ownership Errors
        case ErrorCode::B001_USE_AFTER_MOVE: return "B001";
        case ErrorCode::B002_BORROW_CONFLICT: return "B002";
        case ErrorCode::B003_MUTABILITY_ERROR: return "B003";
        case ErrorCode::B004_MOVE_BORROWED_VALUE: return "B004";
        case ErrorCode::B005_INVALID_BORROW: return "B005";
        case ErrorCode::B006_INVALID_MOVE: return "B006";
        case ErrorCode::B007_INVALID_REFERENCE: return "B007";
        case ErrorCode::B008_INVALID_LIFETIME: return "B008";
        case ErrorCode::B009_INVALID_OWNERSHIP: return "B009";
        case ErrorCode::B010_INVALID_BORROW_CHECK: return "B010";

        // Generic Errors
        case ErrorCode::G001_GENERIC_ERROR: return "G001";
        case ErrorCode::G002_UNKNOWN_ERROR: return "G002";
        case ErrorCode::G003_UNEXPECTED_ERROR: return "G003";
        case ErrorCode::G004_SYSTEM_ERROR: return "G004";
        case ErrorCode::G005_PLATFORM_ERROR: return "G005";
        case ErrorCode::G006_ENVIRONMENT_ERROR: return "G006";
        case ErrorCode::G007_CONFIGURATION_ERROR: return "G007";
        case ErrorCode::G008_INITIALIZATION_ERROR: return "G008";
        case ErrorCode::G009_TERMINATION_ERROR: return "G009";
        case ErrorCode::G010_CLEANUP_ERROR: return "G010";
        case ErrorCode::G011_RESOURCE_ERROR: return "G011";
        case ErrorCode::G012_MEMORY_ERROR: return "G012";
        case ErrorCode::G013_THREAD_ERROR: return "G013";
        case ErrorCode::G014_PROCESS_ERROR: return "G014";
        case ErrorCode::G015_SIGNAL_ERROR: return "G015";
        case ErrorCode::G016_INTERRUPT_ERROR: return "G016";
        case ErrorCode::G017_EXCEPTION_ERROR: return "G017";
        case ErrorCode::G018_ABORT_ERROR: return "G018";
        case ErrorCode::G019_PANIC_ERROR: return "G019";
        case ErrorCode::G020_ASSERTION_ERROR: return "G020";
        case ErrorCode::G021_DEBUG_ERROR: return "G021";
        case ErrorCode::G022_TRACE_ERROR: return "G022";
        case ErrorCode::G023_LOG_ERROR: return "G023";
        case ErrorCode::G024_WARN_ERROR: return "G024";
        case ErrorCode::G025_ERROR_ERROR: return "G025";
        case ErrorCode::G026_FATAL_ERROR: return "G026";
        case ErrorCode::G027_CRITICAL_ERROR: return "G027";
        case ErrorCode::G028_SEVERE_ERROR: return "G028";
        case ErrorCode::G029_EMERGENCY_ERROR: return "G029";
        case ErrorCode::G030_DISASTER_ERROR: return "G030";

        default:
            return "G001"; // Generic/Unknown
        }
    }

    // Updated reportError using Token
    void ErrorHandler::reportError(ErrorCode code, const std::string &message, const lexer::Token &token, ErrorSeverity severity)
    {
        reportError(code, message, std::string(token.filename), token.line, token.column, severity);
    }

    // Updated primary reportError method
    void ErrorHandler::reportError(ErrorCode code, const std::string &message, const std::string &filename,
                                   int line, int column, ErrorSeverity severity)
    {
        std::lock_guard<std::mutex> lock(mutex);
        std::string effectiveFilename = filename.empty() ? defaultFilename : filename;
        errors.emplace_back(code, message, effectiveFilename, line, column, severity);

        if (severity == ErrorSeverity::FATAL)
        {
            fatalErrorFound = true;
        }

        // Improved output format
        std::cerr << effectiveFilename << ":" << line << ":" << column << ": "
                  << (severity == ErrorSeverity::WARNING ? "warning" : "error") // Keep 'error' for ERROR and FATAL for simplicity here
                  << " [" << errorCodeToString(code) << "]: "                   // Include error code
                  << message << std::endl;

        // Optionally: Print code snippet where error occurred if source lines are available
    }

    // Updated reportError for general errors
    void ErrorHandler::reportError(ErrorCode code, const std::string &message, ErrorSeverity severity)
    {
        // Report with unknown location, maybe line 0 or -1
        reportError(code, message, defaultFilename, 0, 0, severity);
    }

    bool ErrorHandler::hasErrors() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        // Check if any error has severity ERROR or FATAL
        for (const auto &err : errors)
        {
            if (err.severity == ErrorSeverity::ERROR || err.severity == ErrorSeverity::FATAL)
            {
                return true;
            }
        }
        return false;
    }

    bool ErrorHandler::hasFatalErrors() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        // Check if any error has severity FATAL
        for (const auto &err : errors)
        {
            if (err.severity == ErrorSeverity::FATAL)
            {
                return true; // Found a fatal error
            }
        }
        // Or return the old flag if you still use setFatal explicitly elsewhere
        // return fatalErrorFound;
        return false; // No fatal severity errors found
    }

    const std::vector<Error> &ErrorHandler::getErrors() const
    {
        // No lock needed if returning const& to immutable data, but depends on usage.
        // Keeping lock for consistency with original code.
        std::lock_guard<std::mutex> lock(mutex);
        return errors;
    }

    // You might remove setFatal and isFatal if you rely solely on the ErrorSeverity::FATAL
    // in the errors vector. Keeping them for now if other logic uses them.
    void ErrorHandler::setFatal(bool fatal)
    {
        std::lock_guard<std::mutex> lock(mutex);
        fatalErrorFound = fatal;
        // Optionally, you could also add a generic FATAL error to the list here
    }

    bool ErrorHandler::isFatal() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return fatalErrorFound || hasFatalErrors(); // Check both flag and list
    }

    void ErrorHandler::clearErrors()
    {
        std::lock_guard<std::mutex> lock(mutex);
        errors.clear();
        fatalErrorFound = false; // Reset fatal flag too
    }

} // namespace error
