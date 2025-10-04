// V8 Integration Tests for Tocin Compiler
// Tests the JavaScript engine integration via V8

#include "../../src/v8_integration/v8_runtime.h"
#include <iostream>
#include <cassert>
#include <string>

using namespace tocin::v8_integration;
using namespace tocin::ffi;

// Test utilities
#define TEST(name) void test_##name()
#define RUN_TEST(name) do { \
    std::cout << "Running test: " #name "..."; \
    test_##name(); \
    std::cout << " PASSED\n"; \
} while(0)

#define ASSERT_TRUE(expr) do { \
    if (!(expr)) { \
        std::cerr << "Assertion failed: " #expr << "\n"; \
        std::cerr << "  at " << __FILE__ << ":" << __LINE__ << "\n"; \
        exit(1); \
    } \
} while(0)

#define ASSERT_FALSE(expr) ASSERT_TRUE(!(expr))
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))

// Test 1: Basic V8 initialization
TEST(v8_initialization) {
    V8Runtime runtime;
    bool initialized = runtime.initialize();
    
    #ifdef WITH_V8
    ASSERT_TRUE(initialized);
    ASSERT_FALSE(runtime.hasError());
    #else
    ASSERT_FALSE(initialized);
    ASSERT_TRUE(runtime.hasError());
    #endif
    
    runtime.shutdown();
}

// Test 2: Simple arithmetic evaluation
TEST(simple_arithmetic) {
    V8Runtime runtime;
    
    #ifdef WITH_V8
    ASSERT_TRUE(runtime.initialize());
    
    auto result = runtime.executeCode("2 + 3");
    ASSERT_FALSE(runtime.hasError());
    ASSERT_TRUE(result.isInt32());
    ASSERT_EQ(result.asInt32(), 5);
    
    runtime.shutdown();
    #endif
}

// Test 3: String operations
TEST(string_operations) {
    V8Runtime runtime;
    
    #ifdef WITH_V8
    ASSERT_TRUE(runtime.initialize());
    
    auto result = runtime.executeCode("'Hello' + ' ' + 'World'");
    ASSERT_FALSE(runtime.hasError());
    ASSERT_TRUE(result.isString());
    ASSERT_EQ(result.asString(), "Hello World");
    
    runtime.shutdown();
    #endif
}

// Test 4: Function definition and call
TEST(function_call) {
    V8Runtime runtime;
    
    #ifdef WITH_V8
    ASSERT_TRUE(runtime.initialize());
    
    // Define function
    runtime.executeCode("function add(a, b) { return a + b; }");
    ASSERT_FALSE(runtime.hasError());
    
    // Call function
    std::vector<FFIValue> args = {FFIValue(10), FFIValue(20)};
    auto result = runtime.callFunction("add", args);
    ASSERT_FALSE(runtime.hasError());
    ASSERT_TRUE(result.isInt32());
    ASSERT_EQ(result.asInt32(), 30);
    
    runtime.shutdown();
    #endif
}

// Test 5: Boolean operations
TEST(boolean_operations) {
    V8Runtime runtime;
    
    #ifdef WITH_V8
    ASSERT_TRUE(runtime.initialize());
    
    auto result = runtime.executeCode("true && false");
    ASSERT_FALSE(runtime.hasError());
    ASSERT_TRUE(result.isBool());
    ASSERT_EQ(result.asBool(), false);
    
    result = runtime.executeCode("true || false");
    ASSERT_TRUE(result.isBool());
    ASSERT_EQ(result.asBool(), true);
    
    runtime.shutdown();
    #endif
}

// Test 6: Array operations
TEST(array_operations) {
    V8Runtime runtime;
    
    #ifdef WITH_V8
    ASSERT_TRUE(runtime.initialize());
    
    auto result = runtime.executeCode("[1, 2, 3].length");
    ASSERT_FALSE(runtime.hasError());
    ASSERT_TRUE(result.isInt32());
    ASSERT_EQ(result.asInt32(), 3);
    
    runtime.shutdown();
    #endif
}

// Test 7: Object operations
TEST(object_operations) {
    V8Runtime runtime;
    
    #ifdef WITH_V8
    ASSERT_TRUE(runtime.initialize());
    
    runtime.executeCode("var obj = { x: 10, y: 20 };");
    auto result = runtime.executeCode("obj.x + obj.y");
    ASSERT_FALSE(runtime.hasError());
    ASSERT_TRUE(result.isInt32());
    ASSERT_EQ(result.asInt32(), 30);
    
    runtime.shutdown();
    #endif
}

// Test 8: Error handling
TEST(error_handling) {
    V8Runtime runtime;
    
    #ifdef WITH_V8
    ASSERT_TRUE(runtime.initialize());
    
    // Invalid syntax
    auto result = runtime.executeCode("function invalid() { return ");
    ASSERT_TRUE(runtime.hasError());
    
    // Undefined variable
    runtime.executeCode("var x = undefinedVariable;");
    ASSERT_TRUE(runtime.hasError());
    
    runtime.shutdown();
    #endif
}

// Test 9: Multiple operations
TEST(multiple_operations) {
    V8Runtime runtime;
    
    #ifdef WITH_V8
    ASSERT_TRUE(runtime.initialize());
    
    runtime.executeCode("var counter = 0;");
    runtime.executeCode("function increment() { counter++; return counter; }");
    
    auto r1 = runtime.callFunction("increment", {});
    ASSERT_EQ(r1.asInt32(), 1);
    
    auto r2 = runtime.callFunction("increment", {});
    ASSERT_EQ(r2.asInt32(), 2);
    
    auto r3 = runtime.callFunction("increment", {});
    ASSERT_EQ(r3.asInt32(), 3);
    
    runtime.shutdown();
    #endif
}

// Test 10: Type conversions
TEST(type_conversions) {
    V8Runtime runtime;
    
    #ifdef WITH_V8
    ASSERT_TRUE(runtime.initialize());
    
    // Number to string
    auto result = runtime.executeCode("String(42)");
    ASSERT_TRUE(result.isString());
    ASSERT_EQ(result.asString(), "42");
    
    // String to number
    result = runtime.executeCode("Number('123')");
    ASSERT_TRUE(result.isInt32());
    ASSERT_EQ(result.asInt32(), 123);
    
    runtime.shutdown();
    #endif
}

int main() {
    std::cout << "=== V8 Integration Tests ===\n\n";
    
    #ifdef WITH_V8
    std::cout << "V8 support is ENABLED\n\n";
    #else
    std::cout << "V8 support is DISABLED (tests will verify error handling)\n\n";
    #endif
    
    RUN_TEST(v8_initialization);
    RUN_TEST(simple_arithmetic);
    RUN_TEST(string_operations);
    RUN_TEST(function_call);
    RUN_TEST(boolean_operations);
    RUN_TEST(array_operations);
    RUN_TEST(object_operations);
    RUN_TEST(error_handling);
    RUN_TEST(multiple_operations);
    RUN_TEST(type_conversions);
    
    std::cout << "\n=== All tests passed! ===\n";
    return 0;
}
