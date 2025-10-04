#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include "../ffi/ffi_value.h"

#ifdef WITH_V8
#include <v8.h>
#include <libplatform/libplatform.h>
#endif

namespace tocin {
namespace v8_integration {

/**
 * @brief V8 JavaScript Runtime for Tocin
 * 
 * This class provides a bridge between Tocin and the V8 JavaScript engine,
 * enabling full JavaScript code execution and interoperability.
 */
class V8Runtime {
public:
    V8Runtime();
    ~V8Runtime();

    // Initialization
    bool initialize();
    void shutdown();

    // Code execution
    ffi::FFIValue executeCode(const std::string& code);
    ffi::FFIValue evaluateExpression(const std::string& expression);

    // Function calls
    ffi::FFIValue callFunction(const std::string& functionName, 
                                const std::vector<ffi::FFIValue>& args);

    // Module management
    bool loadModule(const std::string& modulePath);
    bool registerFunction(const std::string& name, 
                         std::function<ffi::FFIValue(const std::vector<ffi::FFIValue>&)> func);

    // Value conversion
#ifdef WITH_V8
    v8::Local<v8::Value> toV8Value(const ffi::FFIValue& value);
    ffi::FFIValue fromV8Value(v8::Local<v8::Value> value);
#endif

    // Error handling
    std::string getLastError() const { return lastError_; }
    bool hasError() const { return !lastError_.empty(); }

private:
#ifdef WITH_V8
    std::unique_ptr<v8::Platform> platform_;
    v8::Isolate* isolate_;
    v8::Persistent<v8::Context> context_;
    
    // Helper methods
    void setError(const std::string& error);
    void clearError();
    v8::Local<v8::Context> getContext();
#endif

    std::string lastError_;
    bool initialized_;
};

} // namespace v8_integration
} // namespace tocin
