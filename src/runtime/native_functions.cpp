#include "native_functions.h"
#include <iostream>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <sstream>
#include <iomanip>
#include <random>

extern "C" {

// Basic I/O functions
void native_print_string(const char* str) {
    if (str) {
        std::cout << str;
    }
}

void native_print_int(int64_t value) {
    std::cout << value;
}

void native_print_float(double value) {
    std::cout << std::fixed << std::setprecision(6) << value;
}

void native_print_bool(bool value) {
    std::cout << (value ? "true" : "false");
}

void native_println() {
    std::cout << std::endl;
}

// Mathematical functions
double native_sqrt(double value) {
    if (value < 0) {
        return NAN;
    }
    return std::sqrt(value);
}

double native_pow(double base, double exponent) {
    return std::pow(base, exponent);
}

double native_log(double value) {
    if (value <= 0) {
        return NAN;
    }
    return std::log(value);
}

double native_exp(double value) {
    return std::exp(value);
}

double native_sin(double value) {
    return std::sin(value);
}

double native_cos(double value) {
    return std::cos(value);
}

double native_tan(double value) {
    return std::tan(value);
}

double native_asin(double value) {
    if (value < -1 || value > 1) {
        return NAN;
    }
    return std::asin(value);
}

double native_acos(double value) {
    if (value < -1 || value > 1) {
        return NAN;
    }
    return std::acos(value);
}

double native_atan(double value) {
    return std::atan(value);
}

double native_atan2(double y, double x) {
    return std::atan2(y, x);
}

// String manipulation functions
int64_t native_string_length(const char* s) {
    if (!s) return 0;
    return static_cast<int64_t>(std::strlen(s));
}

const char* native_string_concat(const char* s1, const char* s2) {
    if (!s1) s1 = "";
    if (!s2) s2 = "";
    
    size_t len1 = std::strlen(s1);
    size_t len2 = std::strlen(s2);
    size_t total_len = len1 + len2 + 1;
    
    char* result = static_cast<char*>(std::malloc(total_len));
    if (!result) return nullptr;
    
    std::strcpy(result, s1);
    std::strcat(result, s2);
    
    return result;
}

const char* native_int_to_string(int64_t value) {
    std::string str = std::to_string(value);
    char* result = static_cast<char*>(std::malloc(str.length() + 1));
    if (!result) return nullptr;
    
    std::strcpy(result, str.c_str());
    return result;
}

const char* native_float_to_string(double value) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(6) << value;
    std::string str = oss.str();
    
    char* result = static_cast<char*>(std::malloc(str.length() + 1));
    if (!result) return nullptr;
    
    std::strcpy(result, str.c_str());
    return result;
}

int64_t native_string_to_int(const char* str) {
    if (!str) return 0;
    try {
        return std::stoll(str);
    } catch (...) {
        return 0;
    }
}

double native_string_to_float(const char* str) {
    if (!str) return 0.0;
    try {
        return std::stod(str);
    } catch (...) {
        return 0.0;
    }
}

// Memory management
void* native_malloc(size_t size) {
    return std::malloc(size);
}

void native_free(void* ptr) {
    if (ptr) {
        std::free(ptr);
    }
}

// System functions
int64_t native_time() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
    return static_cast<int64_t>(seconds.count());
}

void native_sleep(int64_t milliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

void native_exit(int64_t code) {
    std::exit(static_cast<int>(code));
}

// Random number generation
int64_t native_random_int(int64_t min, int64_t max) {
    if (min > max) {
        int64_t temp = min;
        min = max;
        max = temp;
    }
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int64_t> dis(min, max);
    return dis(gen);
}

double native_random_float(double min, double max) {
    if (min > max) {
        double temp = min;
        min = max;
        max = temp;
    }
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dis(min, max);
    return dis(gen);
}

// Type checking functions
bool native_is_nan(double value) {
    return std::isnan(value);
}

bool native_is_infinite(double value) {
    return std::isinf(value);
}

bool native_is_finite(double value) {
    return std::isfinite(value);
}

// Array/list operations (basic implementations)
void* native_array_create(size_t size, size_t element_size) {
    return std::calloc(size, element_size);
}

void native_array_set(void* array, size_t index, void* value, size_t element_size) {
    if (array && value) {
        char* ptr = static_cast<char*>(array);
        std::memcpy(ptr + index * element_size, value, element_size);
    }
}

void* native_array_get(void* array, size_t index, size_t element_size) {
    if (!array) return nullptr;
    char* ptr = static_cast<char*>(array);
    return ptr + index * element_size;
}

size_t native_array_length(void* array) {
    // This is a simplified implementation
    // In a real implementation, you'd need to track array metadata
    return 0;
}

// Dictionary/map operations (basic implementations)
void* native_dict_create() {
    // Simplified implementation - in practice would use std::unordered_map
    return std::malloc(sizeof(void*));
}

void native_dict_set(void* dict, const char* key, void* value) {
    // Simplified implementation
    // In practice, this would use a proper hash map implementation
}

void* native_dict_get(void* dict, const char* key) {
    // Simplified implementation
    return nullptr;
}

bool native_dict_has(void* dict, const char* key) {
    // Simplified implementation
    return false;
}

// Error handling
void native_panic(const char* message) {
    std::cerr << "PANIC: " << (message ? message : "Unknown error") << std::endl;
    std::exit(1);
}

void native_assert(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "ASSERTION FAILED: " << (message ? message : "Unknown assertion") << std::endl;
        std::exit(1);
    }
}

} // extern "C"
