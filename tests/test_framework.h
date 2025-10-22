#pragma once

#include <string>
#include <vector>
#include <functional>
#include <iostream>
#include <sstream>
#include <chrono>

namespace tocin {
namespace testing {

// Test result structure
struct TestResult {
    std::string test_name;
    bool passed;
    std::string error_message;
    double duration_ms;
};

// Test suite class
class TestSuite {
public:
    using TestFunction = std::function<void()>;
    
    TestSuite(const std::string& name) : name_(name) {}
    
    void add_test(const std::string& test_name, TestFunction test_func) {
        tests_.push_back({test_name, test_func});
    }
    
    std::vector<TestResult> run_all() {
        std::vector<TestResult> results;
        
        std::cout << "Running test suite: " << name_ << std::endl;
        std::cout << "==========================================\n";
        
        for (const auto& test : tests_) {
            auto start = std::chrono::high_resolution_clock::now();
            TestResult result;
            result.test_name = test.first;
            result.passed = true;
            
            try {
                test.second();
            } catch (const std::exception& e) {
                result.passed = false;
                result.error_message = e.what();
            } catch (...) {
                result.passed = false;
                result.error_message = "Unknown exception";
            }
            
            auto end = std::chrono::high_resolution_clock::now();
            result.duration_ms = std::chrono::duration<double, std::milli>(end - start).count();
            
            results.push_back(result);
            
            std::cout << (result.passed ? "✓ PASS" : "✗ FAIL") << ": "
                      << test.first << " (" << result.duration_ms << " ms)";
            if (!result.passed) {
                std::cout << "\n  Error: " << result.error_message;
            }
            std::cout << std::endl;
        }
        
        return results;
    }
    
    const std::string& name() const { return name_; }
    
private:
    std::string name_;
    std::vector<std::pair<std::string, TestFunction>> tests_;
};

// Test registry
class TestRegistry {
public:
    static TestRegistry& instance() {
        static TestRegistry registry;
        return registry;
    }
    
    void add_suite(TestSuite* suite) {
        suites_.push_back(suite);
    }
    
    int run_all(const std::string& filter = "") {
        int total_tests = 0;
        int passed_tests = 0;
        int failed_tests = 0;
        
        for (auto* suite : suites_) {
            if (!filter.empty() && suite->name().find(filter) == std::string::npos) {
                continue;
            }
            
            auto results = suite->run_all();
            for (const auto& result : results) {
                total_tests++;
                if (result.passed) {
                    passed_tests++;
                } else {
                    failed_tests++;
                }
            }
            std::cout << std::endl;
        }
        
        std::cout << "==========================================\n";
        std::cout << "Test Summary:\n";
        std::cout << "  Total:  " << total_tests << "\n";
        std::cout << "  Passed: " << passed_tests << "\n";
        std::cout << "  Failed: " << failed_tests << "\n";
        
        return (failed_tests == 0) ? 0 : 1;
    }
    
private:
    TestRegistry() = default;
    std::vector<TestSuite*> suites_;
};

// Assertion macros
#define ASSERT_TRUE(condition) \
    if (!(condition)) { \
        throw std::runtime_error(std::string("Assertion failed: ") + #condition); \
    }

#define ASSERT_FALSE(condition) \
    if (condition) { \
        throw std::runtime_error(std::string("Assertion failed: not ") + #condition); \
    }

#define ASSERT_EQ(expected, actual) \
    if ((expected) != (actual)) { \
        std::ostringstream oss; \
        oss << "Expected: " << (expected) << ", Actual: " << (actual); \
        throw std::runtime_error(oss.str()); \
    }

#define ASSERT_NE(expected, actual) \
    if ((expected) == (actual)) { \
        std::ostringstream oss; \
        oss << "Expected not equal, but both are: " << (expected); \
        throw std::runtime_error(oss.str()); \
    }

#define ASSERT_THROW(expression, exception_type) \
    { \
        bool caught = false; \
        try { \
            expression; \
        } catch (const exception_type&) { \
            caught = true; \
        } \
        if (!caught) { \
            throw std::runtime_error("Expected exception " #exception_type " was not thrown"); \
        } \
    }

// Test registration helper
#define TEST_SUITE(suite_name) \
    static tocin::testing::TestSuite suite_##suite_name(#suite_name); \
    struct suite_##suite_name##_registrar { \
        suite_##suite_name##_registrar() { \
            tocin::testing::TestRegistry::instance().add_suite(&suite_##suite_name); \
        } \
    }; \
    static suite_##suite_name##_registrar suite_##suite_name##_reg;

#define TEST(suite_name, test_name) \
    void test_##suite_name##_##test_name(); \
    struct test_##suite_name##_##test_name##_registrar { \
        test_##suite_name##_##test_name##_registrar() { \
            suite_##suite_name.add_test(#test_name, test_##suite_name##_##test_name); \
        } \
    }; \
    static test_##suite_name##_##test_name##_registrar test_##suite_name##_##test_name##_reg; \
    void test_##suite_name##_##test_name()

} // namespace testing
} // namespace tocin
