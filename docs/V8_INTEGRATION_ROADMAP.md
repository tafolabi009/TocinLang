# V8 JavaScript Engine Integration Guide

## Overview

This guide provides a comprehensive roadmap for integrating the V8 JavaScript engine into the Tocin compiler, enabling full JavaScript interoperability and execution.

## Current Status

### Completed
- ✅ FFI infrastructure (`src/ffi/ffi_javascript.cpp`)
- ✅ FFI value type system (`src/ffi/ffi_value.cpp`)
- ✅ Module management framework
- ✅ Type conversion system architecture
- ✅ Promise infrastructure design
- ✅ Error handling framework

### Pending
- ⏳ V8 engine initialization
- ⏳ JavaScript code execution
- ⏳ Bidirectional type conversion (Tocin ↔ V8)
- ⏳ Module import/export
- ⏳ Async/await bridge
- ⏳ Event loop integration

## Architecture

### System Design

```
┌────────────────────────────────────────────────────────┐
│                   Tocin Runtime                        │
├────────────────────────────────────────────────────────┤
│                                                        │
│  ┌──────────────────────┐    ┌─────────────────────┐ │
│  │   Tocin Code         │    │   JavaScript Code   │ │
│  │   (.to files)        │    │   (.js files)       │ │
│  └──────────┬───────────┘    └─────────┬───────────┘ │
│             │                           │             │
│             v                           v             │
│  ┌──────────────────────┐    ┌─────────────────────┐ │
│  │  Tocin Interpreter   │◄──►│  V8 Engine          │ │
│  │  or JIT Compiler     │    │  JavaScript VM      │ │
│  └──────────┬───────────┘    └─────────┬───────────┘ │
│             │                           │             │
│             └───────────┬───────────────┘             │
│                         │                             │
│                         v                             │
│              ┌─────────────────────┐                  │
│              │   FFI Bridge        │                  │
│              │  - Type Conversion  │                  │
│              │  - Value Marshaling │                  │
│              │  - Error Handling   │                  │
│              └─────────────────────┘                  │
│                                                        │
└────────────────────────────────────────────────────────┘
```

### Component Interaction

1. **Tocin → JavaScript**:
   - Tocin calls JavaScript function
   - FFI converts Tocin values to V8 values
   - V8 executes JavaScript code
   - Result converted back to Tocin value

2. **JavaScript → Tocin**:
   - JavaScript calls registered Tocin function
   - V8 values converted to FFI values
   - Tocin function executed
   - Result converted to V8 value

## Implementation Roadmap

### Phase 1: V8 Integration Foundation (Week 1-2)

#### 1.1 V8 Installation and Setup

**Prerequisites**:
```bash
# Install V8 dependencies
# Linux
sudo apt-get install libv8-dev

# macOS
brew install v8

# Windows (using vcpkg)
vcpkg install v8
```

**CMake Configuration**:
```cmake
# Add to CMakeLists.txt
option(WITH_V8 "Enable V8 JavaScript Engine" ON)

if(WITH_V8)
    find_package(V8 REQUIRED)
    include_directories(${V8_INCLUDE_DIRS})
    link_directories(${V8_LIBRARY_DIRS})
    
    add_definitions(-DWITH_V8)
    set(V8_LIBRARIES v8 v8_libplatform)
endif()
```

#### 1.2 V8 Initialization

**Implementation**:
```cpp
// src/ffi/v8_runtime.h
#pragma once

#ifdef WITH_V8
#include <v8.h>
#include <libplatform/libplatform.h>
#endif

namespace ffi {

class V8Runtime {
private:
#ifdef WITH_V8
    std::unique_ptr<v8::Platform> platform;
    v8::Isolate* isolate;
    v8::Persistent<v8::Context> context;
#endif
    bool initialized;

public:
    V8Runtime();
    ~V8Runtime();
    
    bool initialize();
    void shutdown();
    
#ifdef WITH_V8
    v8::Isolate* getIsolate() { return isolate; }
    v8::Local<v8::Context> getContext();
#endif
    
    bool isInitialized() const { return initialized; }
};

} // namespace ffi
```

**Implementation**:
```cpp
// src/ffi/v8_runtime.cpp
#include "v8_runtime.h"

#ifdef WITH_V8

namespace ffi {

V8Runtime::V8Runtime() : isolate(nullptr), initialized(false) {
}

V8Runtime::~V8Runtime() {
    shutdown();
}

bool V8Runtime::initialize() {
    if (initialized) return true;
    
    // Initialize V8
    v8::V8::InitializeICUDefaultLocation("");
    v8::V8::InitializeExternalStartupData("");
    
    // Create platform
    platform = v8::platform::NewDefaultPlatform();
    v8::V8::InitializePlatform(platform.get());
    v8::V8::Initialize();
    
    // Create isolate
    v8::Isolate::CreateParams create_params;
    create_params.array_buffer_allocator =
        v8::ArrayBuffer::Allocator::NewDefaultAllocator();
    
    isolate = v8::Isolate::New(create_params);
    
    // Create context
    {
        v8::Isolate::Scope isolate_scope(isolate);
        v8::HandleScope handle_scope(isolate);
        
        v8::Local<v8::Context> ctx = v8::Context::New(isolate);
        context.Reset(isolate, ctx);
    }
    
    initialized = true;
    return true;
}

void V8Runtime::shutdown() {
    if (!initialized) return;
    
    context.Reset();
    
    if (isolate) {
        isolate->Dispose();
        isolate = nullptr;
    }
    
    v8::V8::Dispose();
    v8::V8::DisposePlatform();
    
    initialized = false;
}

v8::Local<v8::Context> V8Runtime::getContext() {
    return context.Get(isolate);
}

} // namespace ffi

#endif // WITH_V8
```

### Phase 2: Type Conversion (Week 3-4)

#### 2.1 Tocin → V8 Conversion

```cpp
// src/ffi/v8_converter.h
#pragma once

#ifdef WITH_V8
#include "ffi_value.h"
#include <v8.h>

namespace ffi {

class V8Converter {
public:
    static v8::Local<v8::Value> toV8(
        v8::Isolate* isolate,
        const FFIValue& value);
    
    static FFIValue fromV8(
        v8::Isolate* isolate,
        v8::Local<v8::Value> value);
    
private:
    static v8::Local<v8::Value> toV8Number(v8::Isolate* isolate, const FFIValue& value);
    static v8::Local<v8::Value> toV8String(v8::Isolate* isolate, const FFIValue& value);
    static v8::Local<v8::Value> toV8Boolean(v8::Isolate* isolate, const FFIValue& value);
    static v8::Local<v8::Value> toV8Array(v8::Isolate* isolate, const FFIValue& value);
    static v8::Local<v8::Value> toV8Object(v8::Isolate* isolate, const FFIValue& value);
    
    static FFIValue fromV8Number(v8::Isolate* isolate, v8::Local<v8::Value> value);
    static FFIValue fromV8String(v8::Isolate* isolate, v8::Local<v8::Value> value);
    static FFIValue fromV8Boolean(v8::Isolate* isolate, v8::Local<v8::Value> value);
    static FFIValue fromV8Array(v8::Isolate* isolate, v8::Local<v8::Array> array);
    static FFIValue fromV8Object(v8::Isolate* isolate, v8::Local<v8::Object> object);
};

} // namespace ffi

#endif // WITH_V8
```

**Implementation**:
```cpp
// src/ffi/v8_converter.cpp
#ifdef WITH_V8

#include "v8_converter.h"

namespace ffi {

v8::Local<v8::Value> V8Converter::toV8(v8::Isolate* isolate, const FFIValue& value) {
    switch (value.getType()) {
        case FFIType::INT32:
            return v8::Integer::New(isolate, value.asInt32());
        
        case FFIType::INT64:
            return v8::Number::New(isolate, static_cast<double>(value.asInt64()));
        
        case FFIType::FLOAT32:
            return v8::Number::New(isolate, value.asFloat32());
        
        case FFIType::FLOAT64:
            return v8::Number::New(isolate, value.asFloat64());
        
        case FFIType::BOOL:
            return v8::Boolean::New(isolate, value.asBool());
        
        case FFIType::STRING:
            return v8::String::NewFromUtf8(isolate, value.asString().c_str())
                .ToLocalChecked();
        
        case FFIType::ARRAY:
            return toV8Array(isolate, value);
        
        case FFIType::OBJECT:
            return toV8Object(isolate, value);
        
        case FFIType::NULL_TYPE:
            return v8::Null(isolate);
        
        default:
            return v8::Undefined(isolate);
    }
}

FFIValue V8Converter::fromV8(v8::Isolate* isolate, v8::Local<v8::Value> value) {
    if (value->IsNumber()) {
        return FFIValue(value->NumberValue(isolate->GetCurrentContext()).ToChecked());
    } else if (value->IsBoolean()) {
        return FFIValue(value->BooleanValue(isolate));
    } else if (value->IsString()) {
        v8::String::Utf8Value utf8(isolate, value);
        return FFIValue(std::string(*utf8, utf8.length()));
    } else if (value->IsArray()) {
        return fromV8Array(isolate, v8::Local<v8::Array>::Cast(value));
    } else if (value->IsObject()) {
        return fromV8Object(isolate, v8::Local<v8::Object>::Cast(value));
    } else if (value->IsNull()) {
        return FFIValue::null();
    } else {
        return FFIValue::undefined();
    }
}

// ... Additional conversion functions

} // namespace ffi

#endif // WITH_V8
```

### Phase 3: JavaScript Execution (Week 5-6)

#### 3.1 Code Execution

```cpp
// Update src/ffi/ffi_javascript.cpp
FFIValue JavaScriptFFIImpl::executeCode(const std::string& code) {
#ifdef WITH_V8
    if (!v8Runtime || !v8Runtime->isInitialized()) {
        return FFIValue::error("V8 runtime not initialized");
    }
    
    auto isolate = v8Runtime->getIsolate();
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);
    
    auto context = v8Runtime->getContext();
    v8::Context::Scope context_scope(context);
    
    // Compile JavaScript code
    v8::Local<v8::String> source =
        v8::String::NewFromUtf8(isolate, code.c_str()).ToLocalChecked();
    
    v8::Local<v8::Script> script;
    if (!v8::Script::Compile(context, source).ToLocal(&script)) {
        return FFIValue::error("JavaScript compilation failed");
    }
    
    // Execute
    v8::Local<v8::Value> result;
    if (!script->Run(context).ToLocal(&result)) {
        return FFIValue::error("JavaScript execution failed");
    }
    
    // Convert result
    return V8Converter::fromV8(isolate, result);
#else
    return FFIValue::error("V8 support not enabled");
#endif
}
```

#### 3.2 Function Calling

```cpp
FFIValue JavaScriptFFIImpl::call(const std::string& name,
                                 const std::vector<FFIValue>& args) {
#ifdef WITH_V8
    auto isolate = v8Runtime->getIsolate();
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);
    
    auto context = v8Runtime->getContext();
    v8::Context::Scope context_scope(context);
    
    // Get function from global scope
    v8::Local<v8::String> funcName =
        v8::String::NewFromUtf8(isolate, name.c_str()).ToLocalChecked();
    
    v8::Local<v8::Value> funcValue;
    if (!context->Global()->Get(context, funcName).ToLocal(&funcValue)) {
        return FFIValue::error("Function not found: " + name);
    }
    
    if (!funcValue->IsFunction()) {
        return FFIValue::error("Not a function: " + name);
    }
    
    v8::Local<v8::Function> func = v8::Local<v8::Function>::Cast(funcValue);
    
    // Convert arguments
    std::vector<v8::Local<v8::Value>> v8Args;
    for (const auto& arg : args) {
        v8Args.push_back(V8Converter::toV8(isolate, arg));
    }
    
    // Call function
    v8::Local<v8::Value> result;
    if (!func->Call(context, context->Global(), v8Args.size(), v8Args.data())
            .ToLocal(&result)) {
        return FFIValue::error("Function call failed");
    }
    
    return V8Converter::fromV8(isolate, result);
#else
    return FFIValue::error("V8 support not enabled");
#endif
}
```

### Phase 4: Module System (Week 7-8)

#### 4.1 Module Loading

```cpp
bool JavaScriptFFIImpl::loadModule(const std::string& moduleName) {
#ifdef WITH_V8
    // Read module file
    std::string moduleCode = readFile(moduleName + ".js");
    if (moduleCode.empty()) {
        return false;
    }
    
    // Wrap in module function
    std::string wrappedCode = 
        "(function(exports, require, module, __filename, __dirname) {\n" +
        moduleCode +
        "\n})";
    
    // Execute module code
    executeCode(wrappedCode);
    
    // Store in loaded modules
    state->loadedModules.insert(moduleName);
    
    return true;
#else
    return false;
#endif
}
```

### Phase 5: Async/Await Bridge (Week 9-10)

#### 5.1 Promise Integration

```cpp
// Promise handling
FFIValue JavaScriptFFIImpl::awaitPromise(const FFIValue& promise) {
#ifdef WITH_V8
    auto isolate = v8Runtime->getIsolate();
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);
    
    auto context = v8Runtime->getContext();
    v8::Context::Scope context_scope(context);
    
    // Convert to V8 promise
    v8::Local<v8::Value> v8Promise = V8Converter::toV8(isolate, promise);
    
    if (!v8Promise->IsPromise()) {
        return FFIValue::error("Not a promise");
    }
    
    // Wait for promise resolution
    v8::Local<v8::Promise> jsPromise = v8::Local<v8::Promise>::Cast(v8Promise);
    
    // Process microtasks until promise settles
    while (jsPromise->State() == v8::Promise::kPending) {
        isolate->PerformMicrotaskCheckpoint();
    }
    
    // Return result
    if (jsPromise->State() == v8::Promise::kFulfilled) {
        return V8Converter::fromV8(isolate, jsPromise->Result());
    } else {
        return FFIValue::error("Promise rejected");
    }
#else
    return FFIValue::error("V8 support not enabled");
#endif
}
```

## Testing Strategy

### Unit Tests

```cpp
// tests/v8_integration_test.cpp
TEST(V8Integration, BasicExecution) {
    V8Runtime runtime;
    ASSERT_TRUE(runtime.initialize());
    
    JavaScriptFFIImpl js(&runtime);
    auto result = js.executeCode("2 + 3");
    
    ASSERT_TRUE(result.isInt32());
    ASSERT_EQ(result.asInt32(), 5);
}

TEST(V8Integration, FunctionCall) {
    V8Runtime runtime;
    ASSERT_TRUE(runtime.initialize());
    
    JavaScriptFFIImpl js(&runtime);
    js.executeCode("function add(a, b) { return a + b; }");
    
    auto result = js.call("add", {FFIValue(10), FFIValue(20)});
    
    ASSERT_TRUE(result.isInt32());
    ASSERT_EQ(result.asInt32(), 30);
}
```

### Integration Tests

```to
// tests/js_ffi_test.to
import ffi.javascript;

// Test basic execution
let result = ffi.javascript.eval("1 + 2");
assert(result == 3);

// Test function calls
ffi.javascript.eval("function greet(name) { return 'Hello, ' + name; }");
let greeting = ffi.javascript.call("greet", ["World"]);
assert(greeting == "Hello, World");

// Test async/await
ffi.javascript.eval("async function delayed() { return 42; }");
let promise = ffi.javascript.call("delayed", []);
let value = await promise;
assert(value == 42);
```

## Performance Considerations

### 1. Object Caching
- Cache frequently used V8 objects
- Reuse contexts when possible
- Pool isolates for parallel execution

### 2. Memory Management
- Properly handle V8 handles
- Use `HandleScope` appropriately
- Monitor memory usage

### 3. Optimization
- Enable V8 JIT compilation
- Use fast property access
- Minimize type conversions

## Deployment

### Build Configuration

```cmake
# Optional V8 integration
option(WITH_V8 "Enable V8 JavaScript Engine" ON)

if(WITH_V8)
    # Check for V8 installation
    find_package(V8 REQUIRED)
    
    # Add V8 to build
    target_link_libraries(tocin PRIVATE ${V8_LIBRARIES})
    target_compile_definitions(tocin PRIVATE WITH_V8)
endif()
```

### Distribution

Include V8 in installers:
- Windows: Bundle V8 DLLs
- Linux: Add V8 to package dependencies
- macOS: Include in application bundle

## Future Enhancements

1. **Node.js API Compatibility**: Support Node.js built-in modules
2. **npm Package Support**: Install and use npm packages
3. **WebAssembly**: Run WASM modules via V8
4. **DevTools Integration**: Chrome DevTools debugging
5. **Performance Profiling**: V8 profiler integration

## Conclusion

This roadmap provides a comprehensive path to integrating V8 into Tocin. The phased approach ensures:
- Stable foundation before complex features
- Testable milestones
- Backward compatibility
- Production-ready implementation

With V8 integration, Tocin will achieve world-class JavaScript interoperability, enabling developers to leverage the entire JavaScript ecosystem.
