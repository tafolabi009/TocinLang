/**
 * @file result.h
 * @brief Rust-style Result and Option types for robust error handling
 * 
 * This module provides type-safe error handling without exceptions,
 * following the Result<T, E> and Option<T> pattern from Rust.
 */

#pragma once

#include <variant>
#include <optional>
#include <stdexcept>
#include <string>
#include <functional>
#include <type_traits>

namespace tocin {
namespace util {

/**
 * @brief Option type - represents an optional value
 * 
 * Similar to std::optional but with Rust-style methods for
 * better composability and error handling.
 */
template<typename T>
class Option {
public:
    Option() : value_(std::nullopt) {}
    Option(std::nullopt_t) : value_(std::nullopt) {}
    Option(const T& value) : value_(value) {}
    Option(T&& value) : value_(std::move(value)) {}
    
    static Option<T> Some(const T& value) {
        return Option(value);
    }
    
    static Option<T> Some(T&& value) {
        return Option(std::move(value));
    }
    
    static Option<T> None() {
        return Option();
    }
    
    bool isSome() const { return value_.has_value(); }
    bool isNone() const { return !value_.has_value(); }
    
    explicit operator bool() const { return isSome(); }
    
    const T& unwrap() const {
        if (!value_.has_value()) {
            throw std::runtime_error("Option::unwrap() called on None value");
        }
        return *value_;
    }
    
    T& unwrap() {
        if (!value_.has_value()) {
            throw std::runtime_error("Option::unwrap() called on None value");
        }
        return *value_;
    }
    
    const T& expect(const char* message) const {
        if (!value_.has_value()) {
            throw std::runtime_error(message);
        }
        return *value_;
    }
    
    T& expect(const char* message) {
        if (!value_.has_value()) {
            throw std::runtime_error(message);
        }
        return *value_;
    }
    
    T unwrapOr(const T& defaultValue) const {
        return value_.value_or(defaultValue);
    }
    
    T unwrapOr(T&& defaultValue) const {
        if (value_.has_value()) {
            return *value_;
        }
        return std::move(defaultValue);
    }
    
    template<typename F>
    T unwrapOrElse(F&& func) const {
        if (value_.has_value()) {
            return *value_;
        }
        return func();
    }
    
    template<typename F>
    auto map(F&& func) const -> Option<decltype(func(std::declval<T>()))> {
        using R = decltype(func(std::declval<T>()));
        if (value_.has_value()) {
            return Option<R>::Some(func(*value_));
        }
        return Option<R>::None();
    }
    
    template<typename F>
    auto andThen(F&& func) const -> decltype(func(std::declval<T>())) {
        if (value_.has_value()) {
            return func(*value_);
        }
        using R = decltype(func(std::declval<T>()));
        return R();
    }
    
    template<typename F>
    Option<T> filter(F&& predicate) const {
        if (value_.has_value() && predicate(*value_)) {
            return *this;
        }
        return None();
    }
    
    const T* getPtr() const {
        return value_.has_value() ? &(*value_) : nullptr;
    }
    
    T* getPtr() {
        return value_.has_value() ? &(*value_) : nullptr;
    }
    
private:
    std::optional<T> value_;
};

/**
 * @brief Result type - represents either success or error
 * 
 * Similar to Rust's Result<T, E>. Forces explicit error handling.
 */
template<typename T, typename E = std::string>
class Result {
public:
    // Constructors
    static Result<T, E> Ok(const T& value) {
        Result<T, E> result;
        result.value_ = value;
        return result;
    }
    
    static Result<T, E> Ok(T&& value) {
        Result<T, E> result;
        result.value_ = std::move(value);
        return result;
    }
    
    static Result<T, E> Err(const E& error) {
        Result<T, E> result;
        result.value_ = error;
        return result;
    }
    
    static Result<T, E> Err(E&& error) {
        Result<T, E> result;
        result.value_ = std::move(error);
        return result;
    }
    
    // Status checks
    bool isOk() const {
        return std::holds_alternative<T>(value_);
    }
    
    bool isErr() const {
        return std::holds_alternative<E>(value_);
    }
    
    explicit operator bool() const { return isOk(); }
    
    // Value access
    const T& unwrap() const {
        if (isErr()) {
            throw std::runtime_error("Result::unwrap() called on Err value");
        }
        return std::get<T>(value_);
    }
    
    T& unwrap() {
        if (isErr()) {
            throw std::runtime_error("Result::unwrap() called on Err value");
        }
        return std::get<T>(value_);
    }
    
    const T& expect(const char* message) const {
        if (isErr()) {
            throw std::runtime_error(message);
        }
        return std::get<T>(value_);
    }
    
    T& expect(const char* message) {
        if (isErr()) {
            throw std::runtime_error(message);
        }
        return std::get<T>(value_);
    }
    
    T unwrapOr(const T& defaultValue) const {
        if (isOk()) {
            return std::get<T>(value_);
        }
        return defaultValue;
    }
    
    T unwrapOr(T&& defaultValue) const {
        if (isOk()) {
            return std::get<T>(value_);
        }
        return std::move(defaultValue);
    }
    
    template<typename F>
    T unwrapOrElse(F&& func) const {
        if (isOk()) {
            return std::get<T>(value_);
        }
        return func(std::get<E>(value_));
    }
    
    // Error access
    const E& unwrapErr() const {
        if (isOk()) {
            throw std::runtime_error("Result::unwrapErr() called on Ok value");
        }
        return std::get<E>(value_);
    }
    
    E& unwrapErr() {
        if (isOk()) {
            throw std::runtime_error("Result::unwrapErr() called on Ok value");
        }
        return std::get<E>(value_);
    }
    
    // Combinators
    template<typename F>
    auto map(F&& func) const -> Result<decltype(func(std::declval<T>())), E> {
        using R = decltype(func(std::declval<T>()));
        if (isOk()) {
            return Result<R, E>::Ok(func(std::get<T>(value_)));
        }
        return Result<R, E>::Err(std::get<E>(value_));
    }
    
    template<typename F>
    auto mapErr(F&& func) const -> Result<T, decltype(func(std::declval<E>()))> {
        using R = decltype(func(std::declval<E>()));
        if (isErr()) {
            return Result<T, R>::Err(func(std::get<E>(value_)));
        }
        return Result<T, R>::Ok(std::get<T>(value_));
    }
    
    template<typename F>
    auto andThen(F&& func) const -> decltype(func(std::declval<T>())) {
        if (isOk()) {
            return func(std::get<T>(value_));
        }
        using R = decltype(func(std::declval<T>()));
        return R::Err(std::get<E>(value_));
    }
    
    template<typename F>
    Result<T, E> orElse(F&& func) const {
        if (isErr()) {
            return func(std::get<E>(value_));
        }
        return *this;
    }
    
    // Conversion to Option
    Option<T> ok() const {
        if (isOk()) {
            return Option<T>::Some(std::get<T>(value_));
        }
        return Option<T>::None();
    }
    
    Option<E> err() const {
        if (isErr()) {
            return Option<E>::Some(std::get<E>(value_));
        }
        return Option<E>::None();
    }
    
    // Pointer access
    const T* getOkPtr() const {
        return isOk() ? &std::get<T>(value_) : nullptr;
    }
    
    T* getOkPtr() {
        return isOk() ? &std::get<T>(value_) : nullptr;
    }
    
    const E* getErrPtr() const {
        return isErr() ? &std::get<E>(value_) : nullptr;
    }
    
    E* getErrPtr() {
        return isErr() ? &std::get<E>(value_) : nullptr;
    }
    
private:
    Result() = default;
    std::variant<T, E> value_;
};

/**
 * @brief Void result - result type for operations that don't return a value
 * 
 * Specialized version of Result<void, E> for operations that only
 * need to signal success/failure.
 */
template<typename E = std::string>
class VoidResult {
public:
    static VoidResult<E> Ok() {
        VoidResult<E> result;
        result.isOk_ = true;
        return result;
    }
    
    static VoidResult<E> Err(const E& error) {
        VoidResult<E> result;
        result.isOk_ = false;
        result.error_ = error;
        return result;
    }
    
    static VoidResult<E> Err(E&& error) {
        VoidResult<E> result;
        result.isOk_ = false;
        result.error_ = std::move(error);
        return result;
    }
    
    bool isOk() const { return isOk_; }
    bool isErr() const { return !isOk_; }
    
    explicit operator bool() const { return isOk(); }
    
    void unwrap() const {
        if (!isOk_) {
            throw std::runtime_error("VoidResult::unwrap() called on Err value");
        }
    }
    
    void expect(const char* message) const {
        if (!isOk_) {
            throw std::runtime_error(message);
        }
    }
    
    const E& unwrapErr() const {
        if (isOk_) {
            throw std::runtime_error("VoidResult::unwrapErr() called on Ok value");
        }
        return error_;
    }
    
    E& unwrapErr() {
        if (isOk_) {
            throw std::runtime_error("VoidResult::unwrapErr() called on Ok value");
        }
        return error_;
    }
    
    Option<E> err() const {
        if (isErr()) {
            return Option<E>::Some(error_);
        }
        return Option<E>::None();
    }
    
private:
    VoidResult() : isOk_(false) {}
    bool isOk_;
    E error_;
};

/**
 * @brief Error type for general compiler errors
 */
struct CompilerError {
    std::string message;
    std::string file;
    int line;
    int column;
    
    CompilerError() : line(0), column(0) {}
    
    CompilerError(const std::string& msg)
        : message(msg), line(0), column(0) {}
    
    CompilerError(const std::string& msg, const std::string& f, int l, int c)
        : message(msg), file(f), line(l), column(c) {}
    
    std::string toString() const {
        if (file.empty()) {
            return message;
        }
        return file + ":" + std::to_string(line) + ":" + 
               std::to_string(column) + ": " + message;
    }
};

// Common result type aliases
template<typename T>
using CompilerResult = Result<T, CompilerError>;

using VoidCompilerResult = VoidResult<CompilerError>;

/**
 * @brief Macro helpers for early return on error
 */
#define TRY(expr) \
    ({ \
        auto __result = (expr); \
        if (__result.isErr()) { \
            return decltype(expr)::Err(std::move(__result.unwrapErr())); \
        } \
        std::move(__result.unwrap()); \
    })

#define TRY_VOID(expr) \
    do { \
        auto __result = (expr); \
        if (__result.isErr()) { \
            return decltype(expr)::Err(std::move(__result.unwrapErr())); \
        } \
    } while(0)

#define TRY_OPTION(expr, error) \
    ({ \
        auto __opt = (expr); \
        if (__opt.isNone()) { \
            return decltype(__result)::Err(error); \
        } \
        std::move(__opt.unwrap()); \
    })

} // namespace util
} // namespace tocin
