#include "ffi_javascript.h"
#include <stdexcept>

namespace tocin {
namespace ffi {

#ifdef WITH_V8
    JavaScriptFFI::JavaScriptFFI() {
        v8::Isolate::CreateParams params;
        allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
        params.array_buffer_allocator = allocator;
        isolate = v8::Isolate::New(params);
        v8::Locker locker(isolate);
        v8::Isolate::Scope isolate_scope(isolate);
        v8::HandleScope handle_scope(isolate);
        auto ctx = v8::Context::New(isolate);
        context.Reset(isolate, ctx);
    }

    JavaScriptFFI::~JavaScriptFFI() {
        context.Reset();
        isolate->Dispose();
        delete allocator;
    }

    v8::Local<v8::Value> JavaScriptFFI::convertToV8Value(
        const FFIValue& value, v8::Isolate* isolate, const v8::Local<v8::Context>& ctx) const {
        switch (value.getType()) {
        case FFIValueType::Int64:
            return v8::Number::New(isolate, static_cast<double>(value.getInt64()));
        case FFIValueType::Double:
            return v8::Number::New(isolate, value.getDouble());
        case FFIValueType::Bool:
            return v8::Boolean::New(isolate, value.getBool());
        case FFIValueType::String:
            return v8::String::NewFromUtf8(isolate, value.getString().c_str()).ToLocalChecked();
        case FFIValueType::Null:
            return v8::Null(isolate);
        case FFIValueType::Array: {
            auto arr = value.getArray();
            auto jsArr = v8::Array::New(isolate, arr.size());
            for (uint32_t i = 0; i < arr.size(); ++i) {
                jsArr->Set(ctx, i, convertToV8Value(arr[i], isolate, ctx)).Check();
            }
            return jsArr;
        }
        case FFIValueType::Object: {
            auto obj = value.getObject();
            auto jsObj = v8::Object::New(isolate);
            for (const auto& [key, val] : obj) {
                jsObj->Set(ctx, v8::String::NewFromUtf8(isolate, key.c_str()).ToLocalChecked(),
                    convertToV8Value(val, isolate, ctx)).Check();
            }
            return jsObj;
        }
        default:
            return v8::Undefined(isolate);
        }
    }

    FFIValue JavaScriptFFI::convertToFFIValue(
        v8::Local<v8::Value> value, v8::Isolate* isolate, const v8::Local<v8::Context>& ctx) const {
        if (value->IsUndefined() || value->IsNull()) return FFIValue();
        if (value->IsInt32()) return FFIValue(static_cast<int64_t>(value->Int32Value(ctx).FromJust()));
        if (value->IsNumber()) return FFIValue(value->NumberValue(ctx).FromJust());
        if (value->IsBoolean()) return FFIValue(value->BooleanValue(isolate));
        if (value->IsString()) {
            v8::String::Utf8Value str(isolate, value);
            return FFIValue(std::string(*str, str.length()));
        }
        if (value->IsArray()) {
            auto jsArr = v8::Local<v8::Array>::Cast(value);
            FFIValue::ArrayType arr;
            arr.reserve(jsArr->Length());
            for (uint32_t i = 0; i < jsArr->Length(); ++i) {
                arr.push_back(convertToFFIValue(
                    jsArr->Get(ctx, i).ToLocalChecked(), isolate, ctx));
            }
            return FFIValue(arr);
        }
        if (value->IsObject()) {
            FFIValue::ObjectType obj;
            auto jsObj = v8::Local<v8::Object>::Cast(value);
            auto keys = jsObj->GetPropertyNames(ctx).ToLocalChecked();
            for (uint32_t i = 0; i < keys->Length(); ++i) {
                auto key = keys->Get(ctx, i).ToLocalChecked();
                v8::String::Utf8Value keyStr(isolate, key);
                auto val = jsObj->Get(ctx, key).ToLocalChecked();
                obj[std::string(*keyStr, keyStr.length())] = convertToFFIValue(val, isolate, ctx);
            }
            return FFIValue(obj);
        }
        return FFIValue();
    }

    FFIValue JavaScriptFFI::call(const std::string& functionName, const std::vector<FFIValue>& args) {
        v8::Locker locker(isolate);
        v8::Isolate::Scope isolate_scope(isolate);
        v8::HandleScope handle_scope(isolate);
        v8::Local<v8::Context> ctx = v8::Local<v8::Context>::New(isolate, context);
        v8::Context::Scope context_scope(ctx);

        auto global = ctx->Global();
        auto func = global->Get(ctx, v8::String::NewFromUtf8(isolate, functionName.c_str()).ToLocalChecked())
            .ToLocalChecked();
        if (!func->IsFunction()) {
            throw std::runtime_error("Function not found: " + functionName);
        }
        auto jsFunc = v8::Local<v8::Function>::Cast(func);
        std::vector<v8::Local<v8::Value>> jsArgs;
        jsArgs.reserve(args.size());
        for (const auto& arg : args) {
            jsArgs.push_back(convertToV8Value(arg, isolate, ctx));
        }
        auto result = jsFunc->Call(ctx, global, jsArgs.size(), jsArgs.data());
        if (result.IsEmpty()) {
            throw std::runtime_error("Error calling JavaScript function: " + functionName);
        }
        return convertToFFIValue(result.ToLocalChecked(), isolate, ctx);
    }

    bool JavaScriptFFI::hasFunction(const std::string& functionName) const {
        v8::Locker locker(isolate);
        v8::Isolate::Scope isolate_scope(isolate);
        v8::HandleScope handle_scope(isolate);
        v8::Local<v8::Context> ctx = v8::Local<v8::Context>::New(isolate, context);
        v8::Context::Scope context_scope(ctx);
        auto global = ctx->Global();
        auto func = global->Get(ctx, v8::String::NewFromUtf8(isolate, functionName.c_str()).ToLocalChecked())
            .ToLocalChecked();
        return func->IsFunction();
    }
#else
    // Enhanced dummy implementations when V8 is not available
    JavaScriptFFI::JavaScriptFFI() { /* Optionally log or set a flag for missing V8 */ }
    JavaScriptFFI::~JavaScriptFFI() {}
    
    FFIValue JavaScriptFFI::call(const std::string& functionName, const std::vector<FFIValue>& args) {
        errorHandler.reportError(error::ErrorCode::F002_FFI_UNAVAILABLE,
            "JavaScript FFI support is not available (V8 not found)",
            error::ErrorSeverity::ERROR);
        return FFIValue();
    }
    
    bool JavaScriptFFI::hasFunction(const std::string& functionName) const {
        return false;
    }
#endif

#ifdef WITH_V8
    V8Engine::V8Engine() {
        v8::V8::InitializeICUDefaultLocation(nullptr);
        v8::V8::InitializeExternalStartupData(nullptr);
        platform_ = v8::platform::NewDefaultPlatform();
        v8::V8::InitializePlatform(platform_.get());
        v8::V8::Initialize();
        
        v8::Isolate::CreateParams create_params;
        create_params.array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
        isolate_ = v8::Isolate::New(create_params);
    }

    V8Engine::~V8Engine() {
        if (isolate_) {
            isolate_->Dispose();
        }
        v8::V8::Dispose();
        v8::V8::ShutdownPlatform();
    }

    tocin::Result<std::string> V8Engine::evaluate(const std::string& code) {
        v8::Locker locker(isolate_);
        v8::Isolate::Scope isolate_scope(isolate_);
        v8::HandleScope handle_scope(isolate_);
        
        v8::Local<v8::Context> context = v8::Context::New(isolate_);
        v8::Context::Scope context_scope(context);
        
        v8::Local<v8::String> source = v8::String::NewFromUtf8(isolate_, code.c_str()).ToLocalChecked();
        v8::Local<v8::Script> script = v8::Script::Compile(context, source).ToLocalChecked();
        v8::Local<v8::Value> result = script->Run(context).ToLocalChecked();
        
        v8::String::Utf8Value utf8_result(isolate_, result);
        return tocin::Result<std::string>::ok(std::string(*utf8_result, utf8_result.length()));
    }
#endif

} // namespace ffi
} // namespace tocin