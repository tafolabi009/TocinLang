#ifndef FFI_JAVASCRIPT_H
#define FFI_JAVASCRIPT_H

#include "ffi_interface.h"
#include <v8.h>

namespace ffi {

    class JavaScriptFFI : public FFIInterface {
    public:
        JavaScriptFFI();
        ~JavaScriptFFI() override;

        FFIValue call(const std::string& functionName, const std::vector<FFIValue>& args) override;
        bool hasFunction(const std::string& functionName) const override;

    private:
        v8::Isolate* isolate;
        v8::Persistent<v8::Context> context;
        v8::ArrayBuffer::Allocator* allocator;

        v8::Local<v8::Value> convertToV8Value(const FFIValue& value, v8::Isolate* isolate,
            const v8::Local<v8::Context>& ctx) const;
        FFIValue convertToFFIValue(v8::Local<v8::Value> value, v8::Isolate* isolate,
            const v8::Local<v8::Context>& ctx) const;
    };

} // namespace ffi

#endif // FFI_JAVASCRIPT_H