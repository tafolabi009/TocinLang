/**
 * Enhanced FFI (Foreign Function Interface) for Tocin
 * Provides support for calling functions in multiple languages
 */

#pragma once

#include "../pch.h"
#include "ffi_value.h"
#include "ffi_cpp.h"
#include "ffi_python.h"
#include "ffi_javascript.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>

namespace ffi
{
    /**
     * @brief Supported target languages for FFI calls
     */
    enum class TargetLanguage
    {
        C,          // Standard C functions
        CPP,        // C++ functions
        PYTHON,     // Python functions
        JAVASCRIPT, // JavaScript functions
        RUST,       // Rust functions
        GO,         // Go functions
        JAVA,       // Java methods
        CSHARP,     // C# methods
        RUBY        // Ruby functions
    };

    /**
     * @brief Structure to hold FFI function information
     */
    struct FFIFunction
    {
        std::string name;
        std::string module;
        TargetLanguage language;
        std::string signature;
        bool isAsync;
        bool isVariadic;

        FFIFunction(
            const std::string &name,
            const std::string &module,
            TargetLanguage language,
            const std::string &signature,
            bool isAsync = false,
            bool isVariadic = false)
            : name(name),
              module(module),
              language(language),
              signature(signature),
              isAsync(isAsync),
              isVariadic(isVariadic)
        {
        }
    };

    /**
     * @brief Main FFI service class
     */
    class FFIService
    {
    public:
        /**
         * @brief Initialize FFI service
         */
        static bool initialize();

        /**
         * @brief Shutdown FFI service
         */
        static void shutdown();

        /**
         * @brief Register a C/C++ function for FFI
         *
         * @param name Function name
         * @param function Native function pointer
         * @param returnType Return type of the function
         * @param paramTypes Parameter types of the function
         * @param isVariadic Whether the function is variadic
         * @return true if registration succeeded
         */
        static bool registerCFunction(
            const std::string &name,
            void *function,
            FFIValueType returnType,
            const std::vector<FFIValueType> &paramTypes,
            bool isVariadic = false);

        /**
         * @brief Register a Python function for FFI
         *
         * @param name Function name
         * @param module Python module name
         * @param function Python function name
         * @param returnType Return type of the function
         * @param paramTypes Parameter types of the function
         * @return true if registration succeeded
         */
        static bool registerPythonFunction(
            const std::string &name,
            const std::string &module,
            const std::string &function,
            FFIValueType returnType,
            const std::vector<FFIValueType> &paramTypes);

        /**
         * @brief Register a JavaScript function for FFI
         *
         * @param name Function name
         * @param module JavaScript module name
         * @param function JavaScript function name
         * @param returnType Return type of the function
         * @param paramTypes Parameter types of the function
         * @param isAsync Whether the function is asynchronous
         * @return true if registration succeeded
         */
        static bool registerJavaScriptFunction(
            const std::string &name,
            const std::string &module,
            const std::string &function,
            FFIValueType returnType,
            const std::vector<FFIValueType> &paramTypes,
            bool isAsync = false);

        /**
         * @brief Call a registered FFI function
         *
         * @param name Function name
         * @param args Function arguments
         * @return FFIValue with the function result
         */
        static FFIValue callFunction(
            const std::string &name,
            const std::vector<FFIValue> &args);

        /**
         * @brief Call a registered FFI function asynchronously
         *
         * @param name Function name
         * @param args Function arguments
         * @param callback Callback to be called with the result
         * @return true if the call was scheduled
         */
        static bool callFunctionAsync(
            const std::string &name,
            const std::vector<FFIValue> &args,
            std::function<void(FFIValue)> callback);

        /**
         * @brief Load an external library
         *
         * @param path Path to the library
         * @return true if the library was loaded successfully
         */
        static bool loadLibrary(const std::string &path);

        /**
         * @brief Check if a function is registered
         *
         * @param name Function name
         * @return true if the function is registered
         */
        static bool isFunctionRegistered(const std::string &name);

        /**
         * @brief Get information about a registered function
         *
         * @param name Function name
         * @return FFIFunction structure with function information
         */
        static FFIFunction getFunctionInfo(const std::string &name);

    private:
        static std::unordered_map<std::string, FFIFunction> registeredFunctions;
        static CPPInterface cppInterface;
        static PythonInterface pythonInterface;
        static JavaScriptInterface jsInterface;
        static bool initialized;
    };

    /**
     * @brief Class for managing language environments
     */
    class LanguageEnvironment
    {
    public:
        /**
         * @brief Initialize a language environment
         *
         * @param language Target language
         * @param config Configuration options
         * @return true if initialization succeeded
         */
        static bool initialize(
            TargetLanguage language,
            const std::unordered_map<std::string, std::string> &config = {});

        /**
         * @brief Shutdown a language environment
         *
         * @param language Target language
         */
        static void shutdown(TargetLanguage language);

        /**
         * @brief Check if a language environment is initialized
         *
         * @param language Target language
         * @return true if the environment is initialized
         */
        static bool isInitialized(TargetLanguage language);

    private:
        static std::unordered_map<TargetLanguage, bool> initializedLanguages;
    };

} // namespace ffi
