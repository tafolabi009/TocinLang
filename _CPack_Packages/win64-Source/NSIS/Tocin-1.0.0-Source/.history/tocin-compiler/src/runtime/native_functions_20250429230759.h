#ifndef NATIVE_FUNCTIONS_H
#define NATIVE_FUNCTIONS_H

#include <cstdint> // For int64_t, bool
#include <cstddef> // For size_t

// --- Forward Declarations for Runtime Types (if needed) ---
// If you have specific runtime representations for Tocin types (like String, List),
// you might need forward declarations or includes here.
// Example:
// struct TocinString;
// struct TocinList;

// --- Native Function Signatures ---
// These are the C++ functions that will be called by the compiled Tocin code.
// They should use C-compatible types for arguments where possible, especially
// for pointers, to simplify LLVM function type creation.

extern "C"
{ // Use C linkage to prevent C++ name mangling

    // --- Basic I/O ---
    /**
     * @brief Prints a null-terminated C string to standard output.
     * @param str The string to print. Handles null gracefully.
     */
    void native_print_string(const char *str);

    /**
     * @brief Prints a 64-bit signed integer to standard output.
     * @param value The integer to print.
     */
    void native_print_int(int64_t value);

    /**
     * @brief Prints a double-precision floating-point number to standard output.
     * @param value The float to print.
     */
    void native_print_float(double value);

    /**
     * @brief Prints a boolean value ("true" or "false") to standard output.
     * @param value The boolean to print.
     */
    void native_print_bool(bool value);

    /**
     * @brief Prints a newline character to standard output.
     */
    void native_println(); // Prints a newline

    // --- Basic Math (Example) ---
    /**
     * @brief Calculates the square root of a double.
     * @param value The number to take the square root of.
     * @return The square root, or NaN for negative input.
     */
    // double native_sqrt(double value);

    // --- String Manipulation (Example - Requires Runtime Memory Management!) ---
    /**
     * @brief Concatenates two C strings.
     * @param s1 The first string.
     * @param s2 The second string.
     * @return A NEWLY ALLOCATED C string containing s1 followed by s2.
     * @warning The caller (Tocin runtime/GC) is responsible for freeing the returned memory!
     */
    // const char* native_string_concat(const char* s1, const char* s2);

    /**
     * @brief Gets the length of a C string.
     * @param s The string.
     * @return The length of the string (number of bytes excluding null terminator).
     */
    // int64_t native_string_length(const char* s);

    // --- Add more native function declarations here ---
    // - File I/O (fopen, fclose, fread, fwrite, fseek, ftell etc.)
    //   - Consider returning status codes or specific error indicators.
    // - Type conversions (int_to_string, float_to_string, string_to_int etc.)
    //   - Need careful error handling for invalid conversions.
    // - Collection operations (if using native runtime types for lists/dicts)
    //   - list_append, list_get, dict_set, dict_get etc.
    // - System functions (time, exit, sleep, get_env)
    // - Memory allocation (if exposing manual allocation to Tocin)

} // extern "C"

#endif // NATIVE_FUNCTIONS_H
