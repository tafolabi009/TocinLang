#include "native_functions.h"
#include <iostream> // For cout, cerr, endl
#include <cmath>    // For sqrt, NAN
#include <string>   // For std::string (used internally if needed)
#include <cstring>  // For strlen, strcpy, strcat
#include <cstdlib>  // For exit, getenv (if implementing system functions)
#include <cstdio>   // For file operations (if implementing file I/O)
#include <new>      // For std::nothrow (safer allocation if not using exceptions)

// --- Native Function Implementations ---

extern "C"
{

    // --- Basic I/O Implementations ---

    void native_print_string(const char *str)
    {
        // Check for null pointer for safety
        if (str)
        {
            std::cout << str;
        }
        else
        {
            // Decide how to represent null in output
            std::cout << "<null_string_ref>";
        }
        // Note: No newline is added automatically by this function.
        // Use native_println() for that.
    }

    void native_print_int(int64_t value)
    {
        std::cout << value;
    }

    void native_print_float(double value)
    {
        // Consider using std::fixed or std::scientific for specific formatting if needed
        std::cout << value;
    }

    void native_print_bool(bool value)
    {
        // Output "true" or "false" strings
        std::cout << (value ? "true" : "false");
    }

    void native_println()
    {
        // Simply output a newline character and flush the stream
        std::cout << std::endl;
    }

    // --- Basic Math Implementation (Example) ---
    /*
    double native_sqrt(double value) {
        if (value < 0.0) {
            // Tocin needs a defined way to handle errors from native calls.
            // Option 1: Return a special value (NaN)
            // Option 2: Set a global error flag/message (requires careful design)
            // Option 3: If Tocin had exceptions, maybe throw one (complex FFI)
            // For now, returning NaN is common for math functions.
            return NAN; // Requires <cmath>
        }
        return std::sqrt(value);
    }
    */

    // --- String Manipulation Implementation (Example - DANGER: Memory Leaks!) ---
    /*
    // WARNING: This is a simplified example and leaks memory badly.
    // A real implementation MUST integrate with Tocin's memory management
    // (e.g., a garbage collector, reference counting, or arena allocation).
    // The Tocin runtime needs to know how to manage the memory returned here.
    const char* native_string_concat(const char* s1, const char* s2) {
        // Handle null inputs gracefully
        if (!s1) s1 = "";
        if (!s2) s2 = "";

        size_t len1 = strlen(s1);
        size_t len2 = strlen(s2);
        // Use std::nothrow to avoid exceptions on allocation failure, check manually
        char* result = new (std::nothrow) char[len1 + len2 + 1];

        if (!result) {
            // Allocation failed! How to report this back to Tocin?
            // Option 1: Return null (caller must check)
            // Option 2: Set a global error state
            // Option 3: Terminate (if it's considered a fatal runtime error)
            std::cerr << "FATAL RUNTIME ERROR: Memory allocation failed in native_string_concat." << std::endl;
            // exit(EXIT_FAILURE); // Drastic, but an option
            return nullptr; // Caller must handle null return
        }

        // Copy the strings
        memcpy(result, s1, len1); // Use memcpy for potentially better performance
        memcpy(result + len1, s2, len2);
        result[len1 + len2] = '\0'; // Null-terminate

        // The pointer 'result' now points to heap-allocated memory.
        // WHO WILL FREE IT? This is the core memory management problem.
        return result;
    }

    int64_t native_string_length(const char* s) {
        // Return 0 for null pointer, otherwise use strlen
        return s ? static_cast<int64_t>(strlen(s)) : 0;
    }
    */

    // --- Add implementations for other native functions here ---
    // Remember to consider:
    // - Error handling (return codes, special values, error flags)
    // - Memory management (who allocates, who frees?)
    // - Thread safety (if Tocin supports threads and they can call native functions)

} // extern "C"
