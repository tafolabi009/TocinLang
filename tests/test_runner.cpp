/**
 * @file test_runner.cpp
 * @brief Comprehensive test runner for the Tocin compiler
 * 
 * This file implements a lightweight but complete test framework
 * that can run all tests, report results, and integrate with CI/CD.
 */

#include <iostream>
#include <vector>
#include <string>
#include <functional>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <memory>
#include <stdexcept>
#include <cstring>
#include <fstream>
#include <cmath>

// Test framework macros
#define TEST_SUITE(name) namespace test_suite_##name {
#define END_TEST_SUITE() }

#define TEST_CASE(name) \
    static void test_##name(); \
    static TestRegistrar registrar_##name(#name, test_##name); \
    static void test_##name()

#define ASSERT_TRUE(expr) do { \
    if (!(expr)) { \
        std::ostringstream oss; \
        oss << "Assertion failed: " << #expr << "\n" \
            << "  File: " << __FILE__ << "\n" \
            << "  Line: " << __LINE__; \
        throw TestFailure(oss.str()); \
    } \
} while(0)

#define ASSERT_FALSE(expr) ASSERT_TRUE(!(expr))
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_NE(a, b) ASSERT_TRUE((a) != (b))
#define ASSERT_LT(a, b) ASSERT_TRUE((a) < (b))
#define ASSERT_LE(a, b) ASSERT_TRUE((a) <= (b))
#define ASSERT_GT(a, b) ASSERT_TRUE((a) > (b))
#define ASSERT_GE(a, b) ASSERT_TRUE((a) >= (b))

#define ASSERT_NEAR(a, b, epsilon) ASSERT_TRUE(std::abs((a) - (b)) < (epsilon))
#define ASSERT_THROW(expr, exception_type) do { \
    bool caught = false; \
    try { expr; } \
    catch (const exception_type&) { caught = true; } \
    catch (...) {} \
    ASSERT_TRUE(caught); \
} while(0)

#define ASSERT_NO_THROW(expr) do { \
    bool threw = false; \
    try { expr; } \
    catch (...) { threw = true; } \
    ASSERT_FALSE(threw); \
} while(0)

// Test exception type
class TestFailure : public std::runtime_error {
public:
    explicit TestFailure(const std::string& message) 
        : std::runtime_error(message) {}
};

// Test result structure
struct TestResult {
    std::string name;
    bool passed;
    std::string error;
    double durationMs;
    
    TestResult(const std::string& n, bool p, const std::string& e, double d)
        : name(n), passed(p), error(e), durationMs(d) {}
};

// Test registry
class TestRegistry {
public:
    using TestFunction = std::function<void()>;
    
    static TestRegistry& instance() {
        static TestRegistry registry;
        return registry;
    }
    
    void registerTest(const std::string& name, TestFunction func) {
        tests_.push_back({name, func});
    }
    
    std::vector<TestResult> runAll() {
        std::vector<TestResult> results;
        
        std::cout << "Running " << tests_.size() << " tests...\n";
        std::cout << std::string(60, '=') << "\n\n";
        
        for (const auto& test : tests_) {
            results.push_back(runTest(test.first, test.second));
        }
        
        return results;
    }
    
    std::vector<TestResult> runFiltered(const std::string& filter) {
        std::vector<TestResult> results;
        
        int matchCount = 0;
        for (const auto& test : tests_) {
            if (test.first.find(filter) != std::string::npos) {
                matchCount++;
            }
        }
        
        std::cout << "Running " << matchCount << " tests matching filter '" 
                  << filter << "'...\n";
        std::cout << std::string(60, '=') << "\n\n";
        
        for (const auto& test : tests_) {
            if (test.first.find(filter) != std::string::npos) {
                results.push_back(runTest(test.first, test.second));
            }
        }
        
        return results;
    }
    
private:
    TestRegistry() = default;
    
    TestResult runTest(const std::string& name, TestFunction func) {
        std::cout << "[ RUN      ] " << name << std::flush;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        try {
            func();
            
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration<double, std::milli>(end - start).count();
            
            std::cout << "\r[       OK ] " << name 
                      << " (" << std::fixed << std::setprecision(2) 
                      << duration << " ms)\n";
            
            return TestResult(name, true, "", duration);
            
        } catch (const TestFailure& e) {
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration<double, std::milli>(end - start).count();
            
            std::cout << "\r[  FAILED  ] " << name 
                      << " (" << duration << " ms)\n";
            std::cout << "  " << e.what() << "\n\n";
            
            return TestResult(name, false, e.what(), duration);
            
        } catch (const std::exception& e) {
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration<double, std::milli>(end - start).count();
            
            std::cout << "\r[  FAILED  ] " << name 
                      << " (" << duration << " ms)\n";
            std::cout << "  Unexpected exception: " << e.what() << "\n\n";
            
            return TestResult(name, false, std::string("Unexpected: ") + e.what(), duration);
            
        } catch (...) {
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration<double, std::milli>(end - start).count();
            
            std::cout << "\r[  FAILED  ] " << name 
                      << " (" << duration << " ms)\n";
            std::cout << "  Unknown exception\n\n";
            
            return TestResult(name, false, "Unknown exception", duration);
        }
    }
    
    std::vector<std::pair<std::string, TestFunction>> tests_;
};

// Test registrar helper
class TestRegistrar {
public:
    TestRegistrar(const std::string& name, TestRegistry::TestFunction func) {
        TestRegistry::instance().registerTest(name, func);
    }
};

// Test reporter
class TestReporter {
public:
    static void printSummary(const std::vector<TestResult>& results) {
        std::cout << "\n" << std::string(60, '=') << "\n";
        
        int passed = 0;
        int failed = 0;
        double totalTime = 0.0;
        
        for (const auto& result : results) {
            if (result.passed) {
                passed++;
            } else {
                failed++;
            }
            totalTime += result.durationMs;
        }
        
        std::cout << "Test Summary:\n";
        std::cout << "  Total:  " << results.size() << "\n";
        std::cout << "  Passed: " << passed << " (" 
                  << std::fixed << std::setprecision(1)
                  << (100.0 * passed / results.size()) << "%)\n";
        std::cout << "  Failed: " << failed << "\n";
        std::cout << "  Time:   " << std::fixed << std::setprecision(2) 
                  << totalTime << " ms\n";
        
        if (failed > 0) {
            std::cout << "\nFailed tests:\n";
            for (const auto& result : results) {
                if (!result.passed) {
                    std::cout << "  - " << result.name << "\n";
                }
            }
        }
        
        std::cout << std::string(60, '=') << "\n";
    }
    
    static void exportJUnit(const std::vector<TestResult>& results, 
                           const std::string& filename) {
        // Export results in JUnit XML format for CI/CD integration
        std::ofstream out(filename);
        if (!out.is_open()) {
            std::cerr << "Failed to open " << filename << " for writing\n";
            return;
        }
        
        int passed = 0;
        int failed = 0;
        double totalTime = 0.0;
        
        for (const auto& result : results) {
            if (result.passed) passed++;
            else failed++;
            totalTime += result.durationMs / 1000.0; // Convert to seconds
        }
        
        out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        out << "<testsuites tests=\"" << results.size() 
            << "\" failures=\"" << failed 
            << "\" time=\"" << totalTime << "\">\n";
        out << "  <testsuite name=\"TocinCompilerTests\" tests=\"" << results.size() 
            << "\" failures=\"" << failed 
            << "\" time=\"" << totalTime << "\">\n";
        
        for (const auto& result : results) {
            out << "    <testcase name=\"" << result.name 
                << "\" time=\"" << (result.durationMs / 1000.0) << "\"";
            
            if (result.passed) {
                out << "/>\n";
            } else {
                out << ">\n";
                out << "      <failure message=\"Test failed\">\n";
                out << result.error << "\n";
                out << "      </failure>\n";
                out << "    </testcase>\n";
            }
        }
        
        out << "  </testsuite>\n";
        out << "</testsuites>\n";
        
        out.close();
        std::cout << "JUnit XML report written to " << filename << "\n";
    }
};

// Main test runner
int main(int argc, char** argv) {
    std::cout << "Tocin Compiler Test Suite\n";
    std::cout << std::string(60, '=') << "\n\n";
    
    std::vector<TestResult> results;
    
    // Parse command-line arguments
    bool exportJUnit = false;
    std::string junitFile = "test_results.xml";
    std::string filter;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--junit" || arg == "-j") {
            exportJUnit = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                junitFile = argv[++i];
            }
        } else if (arg == "--filter" || arg == "-f") {
            if (i + 1 < argc) {
                filter = argv[++i];
            }
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]\n";
            std::cout << "Options:\n";
            std::cout << "  --junit, -j [file]   Export results in JUnit XML format\n";
            std::cout << "  --filter, -f <text>  Run only tests matching filter\n";
            std::cout << "  --help, -h           Show this help message\n";
            return 0;
        }
    }
    
    // Run tests
    if (!filter.empty()) {
        results = TestRegistry::instance().runFiltered(filter);
    } else {
        results = TestRegistry::instance().runAll();
    }
    
    // Print summary
    TestReporter::printSummary(results);
    
    // Export JUnit XML if requested
    if (exportJUnit) {
        TestReporter::exportJUnit(results, junitFile);
    }
    
    // Return exit code
    for (const auto& result : results) {
        if (!result.passed) {
            return 1;
        }
    }
    
    return 0;
}
