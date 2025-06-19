#pragma once

#ifdef WITH_V8
#include <v8.h>
#endif

#include <string>
#include <memory>
#include <vector>
#include "../type/result.h"
#include "ffi_interface.h"
#include "ffi_value.h"

namespace tocin {
namespace ffi {

class JavaScriptFFI : public FFIInterface {
public:
    JavaScriptFFI();
    ~JavaScriptFFI() override;

    FFIValue call(const std::string &functionName, const std::vector<FFIValue> &args) override;
    bool hasFunction(const std::string &functionName) const override;

private:
#ifdef WITH_V8
    v8::Isolate *isolate;
    v8::Persistent<v8::Context> context;
    v8::ArrayBuffer::Allocator *allocator;

    v8::Local<v8::Value> convertToV8Value(const FFIValue &value, v8::Isolate *isolate,
                                          const v8::Local<v8::Context> &ctx) const;
    FFIValue convertToFFIValue(v8::Local<v8::Value> value, v8::Isolate *isolate,
                               const v8::Local<v8::Context> &ctx) const;
#endif
};

class JavaScriptEngine {
public:
    JavaScriptEngine() = default;
    virtual ~JavaScriptEngine() = default;

    virtual tocin::Result<std::string> evaluate(const std::string& code) = 0;
};

#ifdef WITH_V8
class V8Engine : public JavaScriptEngine {
private:
    std::unique_ptr<v8::Platform> platform_;
    v8::Isolate* isolate_ = nullptr;

public:
    V8Engine();
    ~V8Engine() override;

    tocin::Result<std::string> evaluate(const std::string& code) override;
};
#endif

// Fallback implementation when V8 is not available
class DummyJavaScriptEngine : public JavaScriptEngine {
public:
    tocin::Result<std::string> evaluate(const std::string& code) override {
        return tocin::Result<std::string>::error("JavaScript support is not available (V8 not found)");
    }
};

} // namespace ffi
} // namespace tocin
