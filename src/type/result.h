#pragma once

#include <string>
#include <memory>
#include <stdexcept>

namespace tocin {

template<typename T>
class Result {
private:
    bool isSuccess_;
    T value_;
    std::string error_;

public:
    // Default constructor
    Result() : isSuccess_(false), error_("") {}
    
    // Success constructor
    Result(const T& value) : isSuccess_(true), value_(value), error_("") {}
    Result(T&& value) : isSuccess_(true), value_(std::move(value)), error_("") {}

    // Error constructor
    static Result<T> error(const std::string& error) {
        Result<T> result;
        result.isSuccess_ = false;
        result.error_ = error;
        return result;
    }

    // Check if result is success
    bool isSuccess() const {
        return isSuccess_;
    }

    // Check if result is error
    bool isError() const {
        return !isSuccess_;
    }

    // Get value (throws if error)
    const T& getValue() const {
        if (isError()) {
            throw std::runtime_error("Attempting to get value from error result");
        }
        return value_;
    }

    // Get error message
    const std::string& getError() const {
        if (isSuccess()) {
            throw std::runtime_error("Attempting to get error from success result");
        }
        return error_;
    }

    // Implicit conversion to bool
    operator bool() const {
        return isSuccess();
    }
};

} // namespace tocin 