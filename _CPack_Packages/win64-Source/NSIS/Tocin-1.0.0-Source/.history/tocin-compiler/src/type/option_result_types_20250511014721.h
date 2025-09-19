#pragma once

#include "type.h"
#include <memory>
#include <string>
#include <vector>

namespace ast {

// Forward declarations
class Type;
using TypePtr = std::shared_ptr<Type>;

/**
 * Represents an Option type that can be either Some(T) or None.
 * This is used to handle the absence of a value without using null.
 */
class OptionType : public Type {
public:
    explicit OptionType(TypePtr valueType) : valueType(valueType) {}

    std::string toString() const override {
        return "Option<" + valueType->toString() + ">";
    }

    bool equals(const TypePtr& other) const override;
    TypePtr clone() const override;
    
    // Returns the type of the value if present
    TypePtr getValueType() const { return valueType; }

private:
    TypePtr valueType;
};

/**
 * Represents a Result type that can be either Ok(T) or Err(E).
 * This is used for explicit error handling without exceptions.
 */
class ResultType : public Type {
public:
    ResultType(TypePtr okType, TypePtr errType) 
        : okType(okType), errType(errType) {}

    std::string toString() const override {
        return "Result<" + okType->toString() + ", " + errType->toString() + ">";
    }

    bool equals(const TypePtr& other) const override;
    TypePtr clone() const override;

    // Returns the type of the value if successful
    TypePtr getOkType() const { return okType; }
    
    // Returns the type of the error
    TypePtr getErrType() const { return errType; }

private:
    TypePtr okType;   // Type for success case
    TypePtr errType;  // Type for error case
};

} // namespace ast 
