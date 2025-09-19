#pragma once

#include "../ast/types.h"
#include "../error/error_handler.h"
#include <memory>
#include <string>

namespace type_checker {

/**
 * @brief Represents an Option<T> type
 */
class OptionType : public ast::Type {
public:
    ast::TypePtr valueType;

    OptionType(const lexer::Token& token, ast::TypePtr valueType)
        : ast::Type(token), valueType(std::move(valueType)) {}

    std::string toString() const override {
        return "Option<" + valueType->toString() + ">";
    }

    ast::TypePtr clone() const override {
        return std::make_shared<OptionType>(token, valueType->clone());
    }

    static bool isOptionType(ast::TypePtr type) {
        return std::dynamic_pointer_cast<OptionType>(type) != nullptr;
    }

    static ast::TypePtr createOptionType(ast::TypePtr valueType) {
        lexer::Token token; // Default token
        return std::make_shared<OptionType>(token, valueType);
    }
};

/**
 * @brief Represents a Result<T, E> type
 */
class ResultType : public ast::Type {
public:
    ast::TypePtr okType;
    ast::TypePtr errType;

    ResultType(const lexer::Token& token, ast::TypePtr okType, ast::TypePtr errType)
        : ast::Type(token), okType(std::move(okType)), errType(std::move(errType)) {}

    std::string toString() const override {
        return "Result<" + okType->toString() + ", " + errType->toString() + ">";
    }

    ast::TypePtr clone() const override {
        return std::make_shared<ResultType>(token, okType->clone(), errType->clone());
    }

    static bool isResultType(ast::TypePtr type) {
        return std::dynamic_pointer_cast<ResultType>(type) != nullptr;
    }

    static ast::TypePtr createResultType(ast::TypePtr okType, ast::TypePtr errType) {
        lexer::Token token; // Default token
        return std::make_shared<ResultType>(token, okType, errType);
    }
};

/**
 * @brief Handles pattern matching for Result and Option types
 */
class ResultOptionMatcher {
public:
    explicit ResultOptionMatcher(error::ErrorHandler& errorHandler)
        : errorHandler_(errorHandler) {}

    /**
     * @brief Check if a match expression handles all cases for Option/Result
     */
    bool checkExhaustiveness(ast::TypePtr type, const std::vector<std::string>& patterns);

    /**
     * @brief Validate a pattern against an Option/Result type
     */
    bool validatePattern(ast::TypePtr type, const std::string& pattern);

private:
    error::ErrorHandler& errorHandler_;
};

} // namespace type_checker
