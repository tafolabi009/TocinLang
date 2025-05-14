#pragma once

#include "../pch.h"
#include "../ast/ast.h"
#include "../error/error_handler.h"

namespace type_checker
{

/**
 * @brief Enum representing the variants of Option<T>
 * 
 * Similar to Rust's Option, this represents either Some(value) or None
 */
enum class OptionVariant
{
    SOME,
    NONE
};

/**
 * @brief Enum representing the variants of Result<T, E>
 * 
 * Similar to Rust's Result, this represents either Ok(value) or Err(error)
 */
enum class ResultVariant
{
    OK,
    ERR
};

/**
 * @brief Class for checking and verifying Option<T> types
 */
class OptionType
{
public:
    static const std::string TYPE_NAME;

    /**
     * @brief Check if a type is an Option type
     * 
     * @param type The type to check
     * @return true if the type is an Option
     */
    static bool isOptionType(ast::TypePtr type)
    {
        if (auto genericType = std::dynamic_pointer_cast<ast::GenericType>(type))
        {
            return genericType->name == TYPE_NAME;
        }
        return false;
    }

    /**
     * @brief Create an Option type with the given value type
     * 
     * @param valueType The type parameter for Option<T>
     * @return ast::TypePtr The Option type
     */
    static ast::TypePtr createOptionType(ast::TypePtr valueType)
    {
        return std::make_shared<ast::GenericType>(TYPE_NAME, std::vector<ast::TypePtr>{valueType});
    }

    /**
     * @brief Extract the value type from an Option type
     * 
     * @param optionType The Option type
     * @return ast::TypePtr The value type T in Option<T>
     */
    static ast::TypePtr getValueType(ast::TypePtr optionType)
    {
        if (auto genericType = std::dynamic_pointer_cast<ast::GenericType>(optionType))
        {
            if (genericType->name == TYPE_NAME && !genericType->typeArguments.empty())
            {
                return genericType->typeArguments[0];
            }
        }
        return nullptr;
    }
};

/**
 * @brief Class for checking and verifying Result<T, E> types
 */
class ResultType
{
public:
    static const std::string TYPE_NAME;

    /**
     * @brief Check if a type is a Result type
     * 
     * @param type The type to check
     * @return true if the type is a Result
     */
    static bool isResultType(ast::TypePtr type)
    {
        if (auto genericType = std::dynamic_pointer_cast<ast::GenericType>(type))
        {
            return genericType->name == TYPE_NAME;
        }
        return false;
    }

    /**
     * @brief Create a Result type with the given value and error types
     * 
     * @param valueType The Ok value type parameter for Result<T, E>
     * @param errorType The Err error type parameter for Result<T, E>
     * @return ast::TypePtr The Result type
     */
    static ast::TypePtr createResultType(ast::TypePtr valueType, ast::TypePtr errorType)
    {
        return std::make_shared<ast::GenericType>(
            TYPE_NAME, 
            std::vector<ast::TypePtr>{valueType, errorType}
        );
    }

    /**
     * @brief Extract the value type from a Result type
     * 
     * @param resultType The Result type
     * @return ast::TypePtr The value type T in Result<T, E>
     */
    static ast::TypePtr getValueType(ast::TypePtr resultType)
    {
        if (auto genericType = std::dynamic_pointer_cast<ast::GenericType>(resultType))
        {
            if (genericType->name == TYPE_NAME && genericType->typeArguments.size() >= 1)
            {
                return genericType->typeArguments[0];
            }
        }
        return nullptr;
    }

    /**
     * @brief Extract the error type from a Result type
     * 
     * @param resultType The Result type
     * @return ast::TypePtr The error type E in Result<T, E>
     */
    static ast::TypePtr getErrorType(ast::TypePtr resultType)
    {
        if (auto genericType = std::dynamic_pointer_cast<ast::GenericType>(resultType))
        {
            if (genericType->name == TYPE_NAME && genericType->typeArguments.size() >= 2)
            {
                return genericType->typeArguments[1];
            }
        }
        return nullptr;
    }
};

// Define the static const members
const std::string OptionType::TYPE_NAME = "Option";
const std::string ResultType::TYPE_NAME = "Result";

/**
 * @brief Class for pattern matching on Option and Result types
 */
class ResultOptionMatcher
{
public:
    ResultOptionMatcher(error::ErrorHandler &errorHandler)
        : errorHandler(errorHandler) {}

    /**
     * @brief Check if pattern matching on an Option type is valid
     * 
     * @param matchType The type being matched (Option<T>)
     * @param patterns The patterns used in the match expression
     * @return true if the match is valid
     */
    bool checkOptionMatch(ast::TypePtr matchType, const std::vector<ast::PatternPtr> &patterns)
    {
        if (!OptionType::isOptionType(matchType))
        {
            errorHandler.reportError(
                error::ErrorCode::T001_TYPE_MISMATCH,
                "Cannot match on non-Option type",
                "", 0, 0, error::ErrorSeverity::ERROR);
            return false;
        }

        ast::TypePtr valueType = OptionType::getValueType(matchType);
        bool hasSome = false;
        bool hasNone = false;

        for (const auto &pattern : patterns)
        {
            // Check for Some(x) pattern
            if (auto constructorPattern = std::dynamic_pointer_cast<ast::ConstructorPattern>(pattern))
            {
                if (constructorPattern->name == "Some")
                {
                    hasSome = true;
                    // Check that the inner pattern matches the value type
                    if (constructorPattern->patterns.size() != 1)
                    {
                        errorHandler.reportError(
                            error::ErrorCode::T007_INCORRECT_ARGUMENT_COUNT,
                            "Some pattern must have exactly one argument",
                            "", 0, 0, error::ErrorSeverity::ERROR);
                        return false;
                    }
                }
                else if (constructorPattern->name == "None")
                {
                    hasNone = true;
                    // Check that None has no arguments
                    if (!constructorPattern->patterns.empty())
                    {
                        errorHandler.reportError(
                            error::ErrorCode::T007_INCORRECT_ARGUMENT_COUNT,
                            "None pattern must have no arguments",
                            "", 0, 0, error::ErrorSeverity::ERROR);
                        return false;
                    }
                }
            }
            // Allow wildcard pattern
            else if (std::dynamic_pointer_cast<ast::WildcardPattern>(pattern))
            {
                // Wildcard is always valid
                continue;
            }
        }

        // Check exhaustiveness
        if (!hasSome || !hasNone)
        {
            errorHandler.reportError(
                error::ErrorCode::P001_NON_EXHAUSTIVE_PATTERNS,
                "Non-exhaustive patterns: Option match must handle both Some and None cases",
                "", 0, 0, error::ErrorSeverity::ERROR);
            return false;
        }

        return true;
    }

    /**
     * @brief Check if pattern matching on a Result type is valid
     * 
     * @param matchType The type being matched (Result<T, E>)
     * @param patterns The patterns used in the match expression
     * @return true if the match is valid
     */
    bool checkResultMatch(ast::TypePtr matchType, const std::vector<ast::PatternPtr> &patterns)
    {
        if (!ResultType::isResultType(matchType))
        {
            errorHandler.reportError(
                error::ErrorCode::T001_TYPE_MISMATCH,
                "Cannot match on non-Result type",
                "", 0, 0, error::ErrorSeverity::ERROR);
            return false;
        }

        ast::TypePtr valueType = ResultType::getValueType(matchType);
        ast::TypePtr errorType = ResultType::getErrorType(matchType);
        bool hasOk = false;
        bool hasErr = false;

        for (const auto &pattern : patterns)
        {
            // Check for Ok(x) and Err(e) patterns
            if (auto constructorPattern = std::dynamic_pointer_cast<ast::ConstructorPattern>(pattern))
            {
                if (constructorPattern->name == "Ok")
                {
                    hasOk = true;
                    // Check that Ok has exactly one argument
                    if (constructorPattern->patterns.size() != 1)
                    {
                        errorHandler.reportError(
                            error::ErrorCode::T007_INCORRECT_ARGUMENT_COUNT,
                            "Ok pattern must have exactly one argument",
                            "", 0, 0, error::ErrorSeverity::ERROR);
                        return false;
                    }
                }
                else if (constructorPattern->name == "Err")
                {
                    hasErr = true;
                    // Check that Err has exactly one argument
                    if (constructorPattern->patterns.size() != 1)
                    {
                        errorHandler.reportError(
                            error::ErrorCode::T007_INCORRECT_ARGUMENT_COUNT,
                            "Err pattern must have exactly one argument",
                            "", 0, 0, error::ErrorSeverity::ERROR);
                        return false;
                    }
                }
            }
            // Allow wildcard pattern
            else if (std::dynamic_pointer_cast<ast::WildcardPattern>(pattern))
            {
                // Wildcard is always valid
                continue;
            }
        }

        // Check exhaustiveness
        if (!hasOk || !hasErr)
        {
            errorHandler.reportError(
                error::ErrorCode::P001_NON_EXHAUSTIVE_PATTERNS,
                "Non-exhaustive patterns: Result match must handle both Ok and Err cases",
                "", 0, 0, error::ErrorSeverity::ERROR);
            return false;
        }

        return true;
    }

private:
    error::ErrorHandler &errorHandler;
};

} // namespace type_checker 
