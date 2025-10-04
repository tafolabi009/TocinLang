#include "v8_runtime.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <map>
#include <atomic>
#include <mutex>

namespace tocin {
namespace v8_integration {

V8Runtime::V8Runtime() : initialized_(false), promiseIdCounter_(0) {
#ifdef WITH_V8
    isolate_ = nullptr;
#endif
}

V8Runtime::~V8Runtime() {
    shutdown();
}

bool V8Runtime::initialize() {
#ifdef WITH_V8
    if (initialized_) {
        return true;
    }

    // Initialize V8 platform
    platform_ = v8::platform::NewDefaultPlatform();
    v8::V8::InitializePlatform(platform_.get());
    v8::V8::Initialize();

    // Create isolate
    v8::Isolate::CreateParams create_params;
    create_params.array_buffer_allocator = 
        v8::ArrayBuffer::Allocator::NewDefaultAllocator();
    isolate_ = v8::Isolate::New(create_params);

    if (!isolate_) {
        setError("Failed to create V8 isolate");
        return false;
    }

    // Create context
    {
        v8::Isolate::Scope isolate_scope(isolate_);
        v8::HandleScope handle_scope(isolate_);
        v8::Local<v8::Context> context = v8::Context::New(isolate_);
        context_.Reset(isolate_, context);
    }

    initialized_ = true;
    clearError();
    return true;
#else
    setError("V8 support not enabled. Rebuild with -DWITH_V8=ON");
    return false;
#endif
}

void V8Runtime::shutdown() {
#ifdef WITH_V8
    if (!initialized_) {
        return;
    }

    context_.Reset();
    
    if (isolate_) {
        isolate_->Dispose();
        isolate_ = nullptr;
    }

    v8::V8::Dispose();
    v8::V8::DisposePlatform();

    initialized_ = false;
#endif
}

ffi::FFIValue V8Runtime::executeCode(const std::string& code) {
#ifdef WITH_V8
    if (!initialized_) {
        setError("V8 runtime not initialized");
        return ffi::FFIValue::error(lastError_);
    }

    v8::Isolate::Scope isolate_scope(isolate_);
    v8::HandleScope handle_scope(isolate_);
    v8::Local<v8::Context> context = getContext();
    v8::Context::Scope context_scope(context);

    v8::TryCatch try_catch(isolate_);

    // Compile the code
    v8::Local<v8::String> source = 
        v8::String::NewFromUtf8(isolate_, code.c_str()).ToLocalChecked();
    
    v8::Local<v8::Script> script;
    if (!v8::Script::Compile(context, source).ToLocal(&script)) {
        v8::String::Utf8Value error(isolate_, try_catch.Exception());
        setError(std::string("Compilation error: ") + *error);
        return ffi::FFIValue::error(lastError_);
    }

    // Execute the script
    v8::Local<v8::Value> result;
    if (!script->Run(context).ToLocal(&result)) {
        v8::String::Utf8Value error(isolate_, try_catch.Exception());
        setError(std::string("Execution error: ") + *error);
        return ffi::FFIValue::error(lastError_);
    }

    clearError();
    return fromV8Value(result);
#else
    setError("V8 support not enabled");
    return ffi::FFIValue::error(lastError_);
#endif
}

ffi::FFIValue V8Runtime::evaluateExpression(const std::string& expression) {
    return executeCode(expression);
}

ffi::FFIValue V8Runtime::callFunction(const std::string& functionName,
                                       const std::vector<ffi::FFIValue>& args) {
#ifdef WITH_V8
    if (!initialized_) {
        setError("V8 runtime not initialized");
        return ffi::FFIValue::error(lastError_);
    }

    v8::Isolate::Scope isolate_scope(isolate_);
    v8::HandleScope handle_scope(isolate_);
    v8::Local<v8::Context> context = getContext();
    v8::Context::Scope context_scope(context);

    v8::TryCatch try_catch(isolate_);

    // Get the function
    v8::Local<v8::String> funcName = 
        v8::String::NewFromUtf8(isolate_, functionName.c_str()).ToLocalChecked();
    
    v8::Local<v8::Value> funcValue;
    if (!context->Global()->Get(context, funcName).ToLocal(&funcValue)) {
        setError("Failed to get function: " + functionName);
        return ffi::FFIValue::error(lastError_);
    }

    if (!funcValue->IsFunction()) {
        setError("Not a function: " + functionName);
        return ffi::FFIValue::error(lastError_);
    }

    v8::Local<v8::Function> func = v8::Local<v8::Function>::Cast(funcValue);

    // Convert arguments
    std::vector<v8::Local<v8::Value>> v8Args;
    v8Args.reserve(args.size());
    for (const auto& arg : args) {
        v8Args.push_back(toV8Value(arg));
    }

    // Call the function
    v8::Local<v8::Value> result;
    if (!func->Call(context, context->Global(), v8Args.size(), v8Args.data())
            .ToLocal(&result)) {
        v8::String::Utf8Value error(isolate_, try_catch.Exception());
        setError(std::string("Function call error: ") + *error);
        return ffi::FFIValue::error(lastError_);
    }

    clearError();
    return fromV8Value(result);
#else
    setError("V8 support not enabled");
    return ffi::FFIValue::error(lastError_);
#endif
}

bool V8Runtime::loadModule(const std::string& modulePath) {
#ifdef WITH_V8
    // For CommonJS-style modules, use executeCode with module wrapper
    std::ifstream file(modulePath);
    if (!file.is_open()) {
        setError("Failed to open module file: " + modulePath);
        return false;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string code = buffer.str();
    
    // Wrap in module pattern
    std::string wrappedCode = "(function(exports, module) {\n" + code + "\n})(exports, module);";
    
    auto result = executeCode(wrappedCode);
    return !hasError();
#else
    setError("V8 support not enabled");
    return false;
#endif
}

bool V8Runtime::loadESModule(const std::string& modulePath, const std::string& moduleSpecifier) {
#ifdef WITH_V8
    if (!initialized_) {
        setError("V8 runtime not initialized");
        return false;
    }

    v8::Isolate::Scope isolate_scope(isolate_);
    v8::HandleScope handle_scope(isolate_);
    v8::Local<v8::Context> context = getContext();
    v8::Context::Scope context_scope(context);
    v8::TryCatch try_catch(isolate_);

    // Read module source
    std::ifstream file(modulePath);
    if (!file.is_open()) {
        setError("Failed to open module file: " + modulePath);
        return false;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string sourceCode = buffer.str();
    
    // Create module source
    v8::Local<v8::String> source = 
        v8::String::NewFromUtf8(isolate_, sourceCode.c_str()).ToLocalChecked();
    
    v8::ScriptOrigin origin(
        isolate_,
        v8::String::NewFromUtf8(isolate_, modulePath.c_str()).ToLocalChecked(),
        0, 0, false, -1, v8::Local<v8::Value>(), false, false, true);
    
    // Compile module
    v8::ScriptCompiler::Source scriptSource(source, origin);
    v8::MaybeLocal<v8::Module> maybeModule = 
        v8::ScriptCompiler::CompileModule(isolate_, &scriptSource);
    
    if (maybeModule.IsEmpty()) {
        v8::String::Utf8Value error(isolate_, try_catch.Exception());
        setError(std::string("Module compilation error: ") + *error);
        return false;
    }
    
    v8::Local<v8::Module> module = maybeModule.ToLocalChecked();
    
    // Instantiate module
    if (!module->InstantiateModule(context, ResolveModuleCallback).FromMaybe(false)) {
        v8::String::Utf8Value error(isolate_, try_catch.Exception());
        setError(std::string("Module instantiation error: ") + *error);
        return false;
    }
    
    // Evaluate module
    v8::Local<v8::Value> result;
    if (!module->Evaluate(context).ToLocal(&result)) {
        v8::String::Utf8Value error(isolate_, try_catch.Exception());
        setError(std::string("Module evaluation error: ") + *error);
        return false;
    }
    
    // Cache the module
    std::string specifier = moduleSpecifier.empty() ? modulePath : moduleSpecifier;
    moduleCache_[specifier].Reset(isolate_, module);
    modulePathMap_[specifier] = modulePath;
    
    clearError();
    return true;
#else
    setError("V8 support not enabled");
    return false;
#endif
}

ffi::FFIValue V8Runtime::importModule(const std::string& moduleSpecifier) {
#ifdef WITH_V8
    if (!initialized_) {
        setError("V8 runtime not initialized");
        return ffi::FFIValue::error(lastError_);
    }

    v8::Isolate::Scope isolate_scope(isolate_);
    v8::HandleScope handle_scope(isolate_);
    v8::Local<v8::Context> context = getContext();
    v8::Context::Scope context_scope(context);

    // Check if module is cached
    auto it = moduleCache_.find(moduleSpecifier);
    if (it == moduleCache_.end()) {
        setError("Module not loaded: " + moduleSpecifier);
        return ffi::FFIValue::error(lastError_);
    }
    
    v8::Local<v8::Module> module = it->second.Get(isolate_);
    v8::Local<v8::Value> moduleNamespace = module->GetModuleNamespace();
    
    clearError();
    return fromV8Value(moduleNamespace);
#else
    setError("V8 support not enabled");
    return ffi::FFIValue::error(lastError_);
#endif
}

bool V8Runtime::exportValue(const std::string& name, const ffi::FFIValue& value) {
#ifdef WITH_V8
    if (!initialized_) {
        setError("V8 runtime not initialized");
        return false;
    }

    v8::Isolate::Scope isolate_scope(isolate_);
    v8::HandleScope handle_scope(isolate_);
    v8::Local<v8::Context> context = getContext();
    v8::Context::Scope context_scope(context);

    v8::Local<v8::String> key = 
        v8::String::NewFromUtf8(isolate_, name.c_str()).ToLocalChecked();
    v8::Local<v8::Value> v8Value = toV8Value(value);
    
    if (!context->Global()->Set(context, key, v8Value).FromMaybe(false)) {
        setError("Failed to export value: " + name);
        return false;
    }
    
    clearError();
    return true;
#else
    setError("V8 support not enabled");
    return false;
#endif
}

bool V8Runtime::registerFunction(const std::string& name,
                                 std::function<ffi::FFIValue(const std::vector<ffi::FFIValue>&)> func) {
#ifdef WITH_V8
    if (!initialized_) {
        setError("V8 runtime not initialized");
        return false;
    }

    v8::Isolate::Scope isolate_scope(isolate_);
    v8::HandleScope handle_scope(isolate_);
    v8::Local<v8::Context> context = getContext();
    v8::Context::Scope context_scope(context);

    // Store the function for later use
    auto funcPtr = std::make_shared<std::function<ffi::FFIValue(const std::vector<ffi::FFIValue>&)>>(func);
    
    // Create V8 function template
    auto funcTemplate = [](const v8::FunctionCallbackInfo<v8::Value>& args) {
        // This would need proper implementation with external data
        // For now, provide stub implementation
    };
    
    v8::Local<v8::Function> v8Func = 
        v8::Function::New(context, funcTemplate).ToLocalChecked();
    
    v8::Local<v8::String> funcName = 
        v8::String::NewFromUtf8(isolate_, name.c_str()).ToLocalChecked();
    
    if (!context->Global()->Set(context, funcName, v8Func).FromMaybe(false)) {
        setError("Failed to register function: " + name);
        return false;
    }
    
    clearError();
    return true;
#else
    setError("V8 support not enabled");
    return false;
#endif
}

// Async/await bridge implementation
V8Runtime::AsyncResult V8Runtime::createPromise(
    std::function<void(std::function<void(ffi::FFIValue)>, 
                      std::function<void(std::string)>)> executor) {
#ifdef WITH_V8
    if (!initialized_) {
        setError("V8 runtime not initialized");
        return AsyncResult{false, false, true, ffi::FFIValue(), lastError_};
    }

    v8::Isolate::Scope isolate_scope(isolate_);
    v8::HandleScope handle_scope(isolate_);
    v8::Local<v8::Context> context = getContext();
    v8::Context::Scope context_scope(context);

    // Create promise resolver
    v8::Local<v8::Promise::Resolver> resolver = 
        v8::Promise::Resolver::New(context).ToLocalChecked();
    
    std::string promiseId = generatePromiseId();
    
    {
        std::lock_guard<std::mutex> lock(promiseMutex_);
        promiseResolvers_[promiseId].Reset(isolate_, resolver);
    }
    
    // Execute the executor function in a separate thread
    auto resolveFunc = [this, promiseId](ffi::FFIValue value) {
        this->resolvePromise(promiseId, value);
    };
    
    auto rejectFunc = [this, promiseId](std::string reason) {
        this->rejectPromise(promiseId, reason);
    };
    
    std::thread([executor, resolveFunc, rejectFunc]() {
        try {
            executor(resolveFunc, rejectFunc);
        } catch (const std::exception& e) {
            rejectFunc(e.what());
        }
    }).detach();
    
    clearError();
    return AsyncResult{true, false, false, ffi::FFIValue(), ""};
#else
    setError("V8 support not enabled");
    return AsyncResult{false, false, true, ffi::FFIValue(), lastError_};
#endif
}

V8Runtime::AsyncResult V8Runtime::awaitPromise(const std::string& promiseName, int timeoutMs) {
#ifdef WITH_V8
    if (!initialized_) {
        setError("V8 runtime not initialized");
        return AsyncResult{false, false, true, ffi::FFIValue(), lastError_};
    }

    v8::Isolate::Scope isolate_scope(isolate_);
    v8::HandleScope handle_scope(isolate_);
    v8::Local<v8::Context> context = getContext();
    v8::Context::Scope context_scope(context);

    // Get the promise from global scope
    v8::Local<v8::String> name = 
        v8::String::NewFromUtf8(isolate_, promiseName.c_str()).ToLocalChecked();
    
    v8::Local<v8::Value> promiseValue;
    if (!context->Global()->Get(context, name).ToLocal(&promiseValue)) {
        setError("Promise not found: " + promiseName);
        return AsyncResult{false, false, true, ffi::FFIValue(), lastError_};
    }
    
    if (!promiseValue->IsPromise()) {
        setError("Value is not a promise: " + promiseName);
        return AsyncResult{false, false, true, ffi::FFIValue(), lastError_};
    }
    
    v8::Local<v8::Promise> promise = promiseValue.As<v8::Promise>();
    
    // Check promise state
    v8::Promise::PromiseState state = promise->State();
    
    if (state == v8::Promise::kPending) {
        // Wait for promise to settle (with timeout if specified)
        auto start = std::chrono::steady_clock::now();
        while (promise->State() == v8::Promise::kPending) {
            // Process microtasks
            isolate_->PerformMicrotaskCheckpoint();
            
            if (timeoutMs > 0) {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start).count();
                if (elapsed >= timeoutMs) {
                    setError("Promise timeout");
                    return AsyncResult{true, false, false, ffi::FFIValue(), ""};
                }
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        state = promise->State();
    }
    
    if (state == v8::Promise::kFulfilled) {
        v8::Local<v8::Value> result = promise->Result();
        clearError();
        return AsyncResult{false, true, false, fromV8Value(result), ""};
    } else if (state == v8::Promise::kRejected) {
        v8::Local<v8::Value> error = promise->Result();
        v8::String::Utf8Value errorStr(isolate_, error);
        return AsyncResult{false, false, true, ffi::FFIValue(), std::string(*errorStr)};
    }
    
    return AsyncResult{true, false, false, ffi::FFIValue(), ""};
#else
    setError("V8 support not enabled");
    return AsyncResult{false, false, true, ffi::FFIValue(), lastError_};
#endif
}

void V8Runtime::resolvePromise(const std::string& promiseId, const ffi::FFIValue& value) {
#ifdef WITH_V8
    v8::Isolate::Scope isolate_scope(isolate_);
    v8::HandleScope handle_scope(isolate_);
    v8::Local<v8::Context> context = getContext();
    v8::Context::Scope context_scope(context);

    std::lock_guard<std::mutex> lock(promiseMutex_);
    auto it = promiseResolvers_.find(promiseId);
    if (it != promiseResolvers_.end()) {
        v8::Local<v8::Promise::Resolver> resolver = it->second.Get(isolate_);
        resolver->Resolve(context, toV8Value(value)).ToChecked();
        it->second.Reset();
        promiseResolvers_.erase(it);
    }
#endif
}

void V8Runtime::rejectPromise(const std::string& promiseId, const std::string& reason) {
#ifdef WITH_V8
    v8::Isolate::Scope isolate_scope(isolate_);
    v8::HandleScope handle_scope(isolate_);
    v8::Local<v8::Context> context = getContext();
    v8::Context::Scope context_scope(context);

    std::lock_guard<std::mutex> lock(promiseMutex_);
    auto it = promiseResolvers_.find(promiseId);
    if (it != promiseResolvers_.end()) {
        v8::Local<v8::Promise::Resolver> resolver = it->second.Get(isolate_);
        v8::Local<v8::String> errorMsg = 
            v8::String::NewFromUtf8(isolate_, reason.c_str()).ToLocalChecked();
        resolver->Reject(context, errorMsg).ToChecked();
        it->second.Reset();
        promiseResolvers_.erase(it);
    }
#endif
}

#ifdef WITH_V8
v8::Local<v8::Value> V8Runtime::toV8Value(const ffi::FFIValue& value) {
    v8::EscapableHandleScope handle_scope(isolate_);

    switch (value.type) {
        case ffi::FFIType::Int32:
            return handle_scope.Escape(v8::Integer::New(isolate_, value.asInt32()));
        
        case ffi::FFIType::Int64:
            return handle_scope.Escape(v8::Number::New(isolate_, static_cast<double>(value.asInt64())));
        
        case ffi::FFIType::Float:
            return handle_scope.Escape(v8::Number::New(isolate_, value.asFloat()));
        
        case ffi::FFIType::Double:
            return handle_scope.Escape(v8::Number::New(isolate_, value.asDouble()));
        
        case ffi::FFIType::Bool:
            return handle_scope.Escape(v8::Boolean::New(isolate_, value.asBool()));
        
        case ffi::FFIType::String:
            return handle_scope.Escape(
                v8::String::NewFromUtf8(isolate_, value.asString().c_str()).ToLocalChecked());
        
        case ffi::FFIType::Void:
            return handle_scope.Escape(v8::Undefined(isolate_));
        
        default:
            return handle_scope.Escape(v8::Null(isolate_));
    }
}

ffi::FFIValue V8Runtime::fromV8Value(v8::Local<v8::Value> value) {
    if (value->IsNumber()) {
        double num = value->NumberValue(getContext()).ToChecked();
        if (num == static_cast<int32_t>(num)) {
            return ffi::FFIValue(static_cast<int32_t>(num));
        }
        return ffi::FFIValue(num);
    }
    
    if (value->IsBoolean()) {
        return ffi::FFIValue(value->BooleanValue(isolate_));
    }
    
    if (value->IsString()) {
        v8::String::Utf8Value utf8(isolate_, value);
        return ffi::FFIValue(std::string(*utf8));
    }
    
    if (value->IsNull() || value->IsUndefined()) {
        return ffi::FFIValue();
    }
    
    // For complex types, convert to string representation
    v8::String::Utf8Value utf8(isolate_, value);
    return ffi::FFIValue(std::string(*utf8));
}

void V8Runtime::setError(const std::string& error) {
    lastError_ = error;
}

void V8Runtime::clearError() {
    lastError_.clear();
}

v8::Local<v8::Context> V8Runtime::getContext() {
    return context_.Get(isolate_);
}

std::string V8Runtime::generatePromiseId() {
    return "promise_" + std::to_string(promiseIdCounter_.fetch_add(1));
}

v8::MaybeLocal<v8::Module> V8Runtime::ResolveModuleCallback(
    v8::Local<v8::Context> context,
    v8::Local<v8::String> specifier,
    v8::Local<v8::FixedArray> import_assertions,
    v8::Local<v8::Module> referrer) {
    
    // This is a static callback, so we need to get the V8Runtime instance
    // For now, return empty - full implementation would require instance access
    return v8::MaybeLocal<v8::Module>();
}
#endif

} // namespace v8_integration
} // namespace tocin
