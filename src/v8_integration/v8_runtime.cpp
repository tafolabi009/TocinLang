#include "v8_runtime.h"
#include <iostream>
#include <sstream>

namespace tocin {
namespace v8_integration {

V8Runtime::V8Runtime() : initialized_(false) {
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
    // TODO: Implement ES module loading
    setError("Module loading not yet implemented");
    return false;
#else
    setError("V8 support not enabled");
    return false;
#endif
}

bool V8Runtime::registerFunction(const std::string& name,
                                 std::function<ffi::FFIValue(const std::vector<ffi::FFIValue>&)> func) {
#ifdef WITH_V8
    // TODO: Implement function registration
    setError("Function registration not yet implemented");
    return false;
#else
    setError("V8 support not enabled");
    return false;
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
#endif

} // namespace v8_integration
} // namespace tocin
