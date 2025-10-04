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
    
    // ES Module support
    bool loadESModule(const std::string& modulePath, const std::string& moduleSpecifier = "");
    ffi::FFIValue importModule(const std::string& moduleSpecifier);
    bool exportValue(const std::string& name, const ffi::FFIValue& value);
    
    // Async/await bridge
    struct AsyncResult {
        bool isPending;
        bool isResolved;
        bool isRejected;
        ffi::FFIValue value;
        std::string error;
    };
    
    AsyncResult createPromise(std::function<void(std::function<void(ffi::FFIValue)>, 
                                                  std::function<void(std::string)>)> executor);
    AsyncResult awaitPromise(const std::string& promiseName, int timeoutMs = -1);
    void resolvePromise(const std::string& promiseId, const ffi::FFIValue& value);
    void rejectPromise(const std::string& promiseId, const std::string& reason);

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
    
    // Module cache
    std::map<std::string, v8::Persistent<v8::Module>> moduleCache_;
    std::map<std::string, std::string> modulePathMap_;
    
    // Promise tracking
    std::map<std::string, v8::Persistent<v8::Promise::Resolver>> promiseResolvers_;
    std::atomic<uint64_t> promiseIdCounter_;
    std::mutex promiseMutex_;
    
    // Helper methods
    void setError(const std::string& error);
    void clearError();
    v8::Local<v8::Context> getContext();
    std::string generatePromiseId();
    
    // Module resolution callback
    static v8::MaybeLocal<v8::Module> ResolveModuleCallback(
        v8::Local<v8::Context> context,
        v8::Local<v8::String> specifier,
        v8::Local<v8::FixedArray> import_assertions,
        v8::Local<v8::Module> referrer);
#endif

    std::string lastError_;
    bool initialized_;
};

} // namespace v8_integration
} // namespace tocin
