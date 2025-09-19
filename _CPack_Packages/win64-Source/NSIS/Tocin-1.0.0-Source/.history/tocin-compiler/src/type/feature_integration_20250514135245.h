#pragma once

#include "../pch.h"
#include "../ast/ast.h"
#include "../error/error_handler.h"
#include "ownership.h"
#include "result_option.h"
#include "null_safety.h"
#include "extension_functions.h"
#include "traits.h"
#include "move_semantics.h"
#include "../runtime/concurrency.h"
#include <memory>

namespace type_checker
{

    /**
     * @brief Main feature manager for the Tocin language
     *
     * Provides access to all language features in a unified interface.
     */
    class FeatureManager
    {
    public:
        FeatureManager(error::ErrorHandler &errorHandler)
            : errorHandler(errorHandler),
              ownershipChecker(errorHandler),
              resultOptionMatcher(errorHandler),
              nullSafetyChecker(errorHandler),
              extensionManager(errorHandler),
              traitManager(errorHandler),
              moveChecker(errorHandler, ownershipChecker) {}

        // Core feature checkers
        OwnershipChecker ownershipChecker;
        ResultOptionMatcher resultOptionMatcher;
        NullSafetyChecker nullSafetyChecker;
        ExtensionManager extensionManager;
        TraitManager traitManager;
        MoveChecker moveChecker;

        /**
         * @brief Initialize all language features
         *
         * Registers standard library types, traits, and extension methods.
         */
        void initialize()
        {
            // Register standard result/option types
            // Register standard traits
            // Register standard extension methods
            // etc.
        }

        /**
         * @brief Enter a new scope for ownership tracking
         */
        void enterScope()
        {
            ownershipChecker.enterScope();
        }

        /**
         * @brief Exit the current scope for ownership tracking
         */
        void exitScope()
        {
            ownershipChecker.exitScope();
        }

        /**
         * @brief Check if type contains any of the advanced features
         *
         * @param type The type to check
         * @return true if the type uses Result, Option, nullable, etc.
         */
        bool usesAdvancedFeatures(ast::TypePtr type)
        {
            // Check for Result type
            if (type_checker::ResultType::isResultType(type))
            {
                return true;
            }

            // Check for Option type
            if (type_checker::OptionType::isOptionType(type))
            {
                return true;
            }

            // Check for nullable type
            if (nullSafetyChecker.isNullableType(type))
            {
                return true;
            }

            // Check for trait type
            if (std::dynamic_pointer_cast<DynTraitType>(type))
            {
                return true;
            }

            // Check for rvalue reference type
            if (RValueReference::isRValueRefType(type))
            {
                return true;
            }

            // For generic types, check type arguments
            if (auto genericType = std::dynamic_pointer_cast<ast::GenericType>(type))
            {
                for (const auto &typeArg : genericType->typeArguments)
                {
                    if (usesAdvancedFeatures(typeArg))
                    {
                        return true;
                    }
                }
            }

            return false;
        }

        /**
         * @brief Analyze a function for advanced feature usage
         *
         * @param function The function to analyze
         * @return true if the function uses any advanced features
         */
        bool analyzeFunctionFeatures(ast::FunctionStmt *function)
        {
            bool usesFeatures = false;

            // Check return type
            if (usesAdvancedFeatures(function->returnType))
            {
                usesFeatures = true;
            }

            // Check parameter types
            for (const auto &param : function->parameters)
            {
                if (usesAdvancedFeatures(param.type))
                {
                    usesFeatures = true;
                }

                // Check for move semantics
                if (param.isMoved)
                {
                    usesFeatures = true;
                }
            }

            // In practice, would also analyze the function body for:
            // - Move expressions
            // - Channel operations
            // - Defer statements
            // - Safe call operators
            // - etc.

            return usesFeatures;
        }

        /**
         * @brief Create an Option<T> type
         *
         * @param valueType The type of value to wrap
         * @return ast::TypePtr The Option type
         */
        ast::TypePtr createOptionType(ast::TypePtr valueType)
        {
            return OptionType::createOptionType(valueType);
        }

        /**
         * @brief Create a Result<T, E> type
         *
         * @param valueType The success type
         * @param errorType The error type
         * @return ast::TypePtr The Result type
         */
        ast::TypePtr createResultType(ast::TypePtr valueType, ast::TypePtr errorType)
        {
            return ResultType::createResultType(valueType, errorType);
        }

        /**
         * @brief Create a nullable type
         *
         * @param baseType The type to make nullable
         * @return ast::TypePtr The nullable type
         */
        ast::TypePtr createNullableType(ast::TypePtr baseType)
        {
            return nullSafetyChecker.makeNullable(baseType);
        }

        /**
         * @brief Create a channel type
         *
         * @param elementType The type of elements in the channel
         * @return ast::TypePtr The channel type
         */
        ast::TypePtr createChannelType(ast::TypePtr elementType)
        {
            return runtime::ChannelType::createChannelType(elementType);
        }

        /**
         * @brief Create a dynamic trait object type
         *
         * @param traitType The trait interface
         * @return ast::TypePtr The dynamic trait object type
         */
        ast::TypePtr createDynamicTraitType(ast::TypePtr traitType)
        {
            lexer::Token token; // Create a default token
            return std::make_shared<DynTraitType>(token, traitType);
        }

        /**
         * @brief Create an rvalue reference type
         *
         * @param baseType The base type
         * @return ast::TypePtr The rvalue reference type
         */
        ast::TypePtr createRValueRefType(ast::TypePtr baseType)
        {
            return RValueReference::createRValueRefType(baseType);
        }

    private:
        error::ErrorHandler &errorHandler;
    };

} // namespace type_checker
