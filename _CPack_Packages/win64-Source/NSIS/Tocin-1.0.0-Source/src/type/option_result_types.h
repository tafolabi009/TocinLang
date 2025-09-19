#pragma once

#include "../ast/types.h"
#include "../ast/ast.h"
#include "../error/error_handler.h"
#include "result_option.h"
#include <memory>
#include <string>
#include <functional>
#include <utility>

namespace type_checker {

/**
 * @brief Option type implementation for null safety
 */
template<typename T>
class Option {
public:
    // Constructors
    Option() : hasValue_(false) {}
    Option(const T& value) : hasValue_(true), value_(value) {}
    Option(T&& value) : hasValue_(true), value_(std::move(value)) {}
    
    // Copy and move constructors
    Option(const Option& other) : hasValue_(other.hasValue_), value_(other.value_) {}
    Option(Option&& other) noexcept : hasValue_(other.hasValue_), value_(std::move(other.value_)) {
        other.hasValue_ = false;
    }

    // Assignment operators
    Option& operator=(const Option& other) {
        if (this != &other) {
            hasValue_ = other.hasValue_;
            value_ = other.value_;
        }
        return *this;
    }

    Option& operator=(Option&& other) noexcept {
        if (this != &other) {
            hasValue_ = other.hasValue_;
            value_ = std::move(other.value_);
            other.hasValue_ = false;
        }
        return *this;
    }

    // Static factory methods
    static Option<T> Some(const T& value) { return Option<T>(value); }
    static Option<T> Some(T&& value) { return Option<T>(std::move(value)); }
    static Option<T> None() { return Option<T>(); }

    // Value checking
    bool isSome() const { return hasValue_; }
    bool isNone() const { return !hasValue_; }
    explicit operator bool() const { return hasValue_; }

    // Value access
    const T& unwrap() const {
        if (!hasValue_) {
            throw std::runtime_error("Called unwrap on None value");
        }
        return value_;
    }

    T& unwrap() {
        if (!hasValue_) {
            throw std::runtime_error("Called unwrap on None value");
        }
        return value_;
    }

    const T& unwrapOr(const T& defaultValue) const {
        return hasValue_ ? value_ : defaultValue;
    }

    T unwrapOrElse(std::function<T()> defaultFunc) const {
        return hasValue_ ? value_ : defaultFunc();
    }

    // Functional operations
    template<typename F>
    auto map(F func) const -> Option<decltype(func(std::declval<T>()))> {
        using ReturnType = decltype(func(std::declval<T>()));
        if (hasValue_) {
            return Option<ReturnType>::Some(func(value_));
        }
        return Option<ReturnType>::None();
    }

    template<typename F>
    auto flatMap(F func) const -> decltype(func(std::declval<T>())) {
        if (hasValue_) {
            return func(value_);
        }
        return decltype(func(std::declval<T>()))::None();
    }

    template<typename F>
    Option<T> filter(F predicate) const {
        if (hasValue_ && predicate(value_)) {
            return *this;
        }
        return Option<T>::None();
    }

    // Comparison operators
    bool operator==(const Option<T>& other) const {
        if (hasValue_ != other.hasValue_) return false;
        if (!hasValue_) return true;
        return value_ == other.value_;
    }

    bool operator!=(const Option<T>& other) const {
        return !(*this == other);
    }

private:
    bool hasValue_;
    T value_;
};

/**
 * @brief Result type implementation for error handling
 */
template<typename T, typename E = std::string>
class Result {
public:
    // Constructors
    Result(const T& value) : isOk_(true), value_(value) {}
    Result(T&& value) : isOk_(true), value_(std::move(value)) {}
    Result(const E& error) : isOk_(false), error_(error) {}
    Result(E&& error) : isOk_(false), error_(std::move(error)) {}

    // Copy and move constructors
    Result(const Result& other) : isOk_(other.isOk_) {
        if (isOk_) {
            value_ = other.value_;
        } else {
            error_ = other.error_;
        }
    }

    Result(Result&& other) noexcept : isOk_(other.isOk_) {
        if (isOk_) {
            value_ = std::move(other.value_);
        } else {
            error_ = std::move(other.error_);
        }
    }

    // Assignment operators
    Result& operator=(const Result& other) {
        if (this != &other) {
            isOk_ = other.isOk_;
            if (isOk_) {
                value_ = other.value_;
            } else {
                error_ = other.error_;
            }
        }
        return *this;
    }

    Result& operator=(Result&& other) noexcept {
        if (this != &other) {
            isOk_ = other.isOk_;
            if (isOk_) {
                value_ = std::move(other.value_);
            } else {
                error_ = std::move(other.error_);
            }
        }
        return *this;
    }

    // Static factory methods
    static Result<T, E> Ok(const T& value) { return Result<T, E>(value); }
    static Result<T, E> Ok(T&& value) { return Result<T, E>(std::move(value)); }
    static Result<T, E> Err(const E& error) { return Result<T, E>(error); }
    static Result<T, E> Err(E&& error) { return Result<T, E>(std::move(error)); }

    // State checking
    bool isOk() const { return isOk_; }
    bool isErr() const { return !isOk_; }
    explicit operator bool() const { return isOk_; }

    // Value access
    const T& unwrap() const {
        if (!isOk_) {
            throw std::runtime_error("Called unwrap on Err value");
        }
        return value_;
    }

    T& unwrap() {
        if (!isOk_) {
            throw std::runtime_error("Called unwrap on Err value");
        }
        return value_;
    }

    const E& unwrapErr() const {
        if (isOk_) {
            throw std::runtime_error("Called unwrapErr on Ok value");
        }
        return error_;
    }

    E& unwrapErr() {
        if (isOk_) {
            throw std::runtime_error("Called unwrapErr on Ok value");
        }
        return error_;
    }

    const T& unwrapOr(const T& defaultValue) const {
        return isOk_ ? value_ : defaultValue;
    }

    T unwrapOrElse(std::function<T(const E&)> errorFunc) const {
        return isOk_ ? value_ : errorFunc(error_);
    }

    // Functional operations
    template<typename F>
    auto map(F func) const -> Result<decltype(func(std::declval<T>())), E> {
        using ReturnType = decltype(func(std::declval<T>()));
        if (isOk_) {
            return Result<ReturnType, E>::Ok(func(value_));
        }
        return Result<ReturnType, E>::Err(error_);
    }

    template<typename F>
    auto mapErr(F func) const -> Result<T, decltype(func(std::declval<E>()))> {
        using ErrorType = decltype(func(std::declval<E>()));
        if (isOk_) {
            return Result<T, ErrorType>::Ok(value_);
        }
        return Result<T, ErrorType>::Err(func(error_));
    }

    template<typename F>
    auto flatMap(F func) const -> decltype(func(std::declval<T>())) {
        if (isOk_) {
            return func(value_);
        }
        return decltype(func(std::declval<T>()))::Err(error_);
    }

    // Conversion to Option
    Option<T> ok() const {
        if (isOk_) {
            return Option<T>::Some(value_);
        }
        return Option<T>::None();
    }

    Option<E> err() const {
        if (!isOk_) {
            return Option<E>::Some(error_);
        }
        return Option<E>::None();
    }

    // Comparison operators
    bool operator==(const Result<T, E>& other) const {
        if (isOk_ != other.isOk_) return false;
        if (isOk_) {
            return value_ == other.value_;
        } else {
            return error_ == other.error_;
        }
    }

    bool operator!=(const Result<T, E>& other) const {
        return !(*this == other);
    }

private:
    bool isOk_;
    union {
        T value_;
        E error_;
    };
};

/**
 * @brief Option and Result type utilities
 */
class OptionResultUtils {
public:
    // Option utilities
    template<typename T>
    static Option<T> fromPointer(T* ptr) {
        return ptr ? Option<T>::Some(*ptr) : Option<T>::None();
    }

    template<typename T>
    static Option<T> fromNullable(const T* ptr) {
        return ptr ? Option<T>::Some(*ptr) : Option<T>::None();
    }

    // Result utilities
    template<typename T>
    static Result<T> fromException(std::function<T()> func) {
        try {
            return Result<T>::Ok(func());
        } catch (const std::exception& e) {
            return Result<T>::Err(std::string(e.what()));
        }
    }

    template<typename T, typename E>
    static Result<T, E> fromBool(bool success, const T& value, const E& error) {
        return success ? Result<T, E>::Ok(value) : Result<T, E>::Err(error);
    }

    // Combination utilities
    template<typename T, typename U>
    static Option<std::pair<T, U>> combine(const Option<T>& opt1, const Option<U>& opt2) {
        if (opt1.isSome() && opt2.isSome()) {
            return Option<std::pair<T, U>>::Some(std::make_pair(opt1.unwrap(), opt2.unwrap()));
        }
        return Option<std::pair<T, U>>::None();
    }

    template<typename T, typename U, typename E>
    static Result<std::pair<T, U>, E> combine(const Result<T, E>& res1, const Result<U, E>& res2) {
        if (res1.isOk() && res2.isOk()) {
            return Result<std::pair<T, U>, E>::Ok(std::make_pair(res1.unwrap(), res2.unwrap()));
        }
        if (res1.isErr()) {
            return Result<std::pair<T, U>, E>::Err(res1.unwrapErr());
        }
        return Result<std::pair<T, U>, E>::Err(res2.unwrapErr());
    }

    // Collection utilities
    template<typename T>
    static Option<std::vector<T>> sequence(const std::vector<Option<T>>& options) {
        std::vector<T> result;
        result.reserve(options.size());
        
        for (const auto& opt : options) {
            if (opt.isNone()) {
                return Option<std::vector<T>>::None();
            }
            result.push_back(opt.unwrap());
        }
        
        return Option<std::vector<T>>::Some(std::move(result));
    }

    template<typename T, typename E>
    static Result<std::vector<T>, E> sequence(const std::vector<Result<T, E>>& results) {
        std::vector<T> values;
        values.reserve(results.size());
        
        for (const auto& res : results) {
            if (res.isErr()) {
                return Result<std::vector<T>, E>::Err(res.unwrapErr());
            }
            values.push_back(res.unwrap());
        }
        
        return Result<std::vector<T>, E>::Ok(std::move(values));
    }

private:
    OptionResultUtils() = delete;
};

} // namespace type_checker
