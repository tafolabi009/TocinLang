# Foreign Function Interface (FFI) in Tocin

Tocin provides seamless interoperability with Python, JavaScript (via V8), and C++. This allows you to call functions from these languages directly in Tocin code, enabling powerful integrations and reuse of existing libraries.

## Python FFI

### Usage
```to
let py_result = ffi.python.call("len", [[1, 2, 3]]);
print("Python len([1,2,3]) = " + py_result);
```
- Ensure Python 3.6+ is installed and enabled in your build.

## JavaScript FFI

### Usage
```to
let js_result = ffi.javascript.call("eval", ["1+2+3"]);
print("JavaScript eval('1+2+3') = " + js_result);
```
- Requires Node.js 12+ and V8 support enabled in your build.

## C++ FFI

### Usage
Register C++ functions in the runtime, then call from Tocin:
```cpp
// C++ side
ffi::CppFFI::registerFunction("add", [](const std::vector<FFIValue>& args) {
    return FFIValue(args[0].getInt64() + args[1].getInt64());
});
```
```to
let cpp_result = ffi.cpp.call("add", [1, 2]);
print("C++ add(1,2) = " + cpp_result);
```

## Error Handling
- If a backend (Python, JS, C++) is not available, a clear error is reported.
- If a function is missing or arguments are invalid, an error is reported.

## Best Practices
- Always check for FFI errors and handle gracefully.
- Use FFI for performance-critical or library integration tasks.
- Avoid overusing FFI for simple logic—prefer native Tocin code for maintainability.

## Troubleshooting
- **FFI not available:** Ensure Python/Node.js are installed and enabled in CMake.
- **Function not found:** Check the function name and registration.
- **Type mismatch:** Ensure argument types match the expected signature.

See also: [Advanced Topics](05_Advanced_Topics.md) 