#pragma once

#include "../pch.h"
#include "../ast/ast.h"
#include "../error/error_handler.h"

namespace type_checker
{

    /**
     * @brief Class for managing extension functions
     *
     * Extension functions allow adding methods to existing types without
     * modifying the original type, similar to Kotlin's extension functions.
     */
    class ExtensionManager
    {
    public:
        ExtensionManager(error::ErrorHandler &errorHandler)
            : errorHandler(errorHandler) {}

        /**
         * @brief Register an extension function for a type
         *
         * @param targetType The type being extended
         * @param functionName The name of the extension function
         * @param function The function being registered
         * @return true if registration succeeded
         */
        bool registerExtension(ast::TypePtr targetType, const std::string &functionName,
                               ast::FunctionStmt *function)
        {
            std::string typeName = targetType->toString();

            // Check if this extension already exists
            auto &extensionsForType = extensions[typeName];
            if (extensionsForType.find(functionName) != extensionsForType.end())
            {
                errorHandler.reportError(
                    error::ErrorCode::T003_UNDEFINED_FUNCTION,
                    "Extension function '" + functionName + "' already defined for type '" + typeName + "'",
                    "", 0, 0, error::ErrorSeverity::ERROR);
                return false;
            }

            // Register the extension
            extensionsForType[functionName] = function;

            return true;
        }

        /**
         * @brief Look up an extension function for a type
         *
         * @param targetType The type being extended
         * @param functionName The name of the extension function
         * @return ast::FunctionStmt* The function, or nullptr if not found
         */
        ast::FunctionStmt *findExtension(ast::TypePtr targetType, const std::string &functionName)
        {
            std::string typeName = targetType->toString();

            // Look for extensions for this exact type
            auto typeIt = extensions.find(typeName);
            if (typeIt != extensions.end())
            {
                auto funcIt = typeIt->second.find(functionName);
                if (funcIt != typeIt->second.end())
                {
                    return funcIt->second;
                }
            }

            // If not found, check base types for inheritance
            // This is a simplified approach - real implementation would have more detail

            return nullptr;
        }

        /**
         * @brief Get all extension functions for a type
         *
         * @param targetType The type to find extensions for
         * @return std::vector<std::string> Names of available extension functions
         */
        std::vector<std::string> getExtensionsForType(ast::TypePtr targetType)
        {
            std::string typeName = targetType->toString();
            std::vector<std::string> result;

            auto typeIt = extensions.find(typeName);
            if (typeIt != extensions.end())
            {
                for (const auto &[name, func] : typeIt->second)
                {
                    result.push_back(name);
                }
            }

            return result;
        }

    private:
        // Map of type name -> (function name -> function)
        std::map<std::string, std::map<std::string, ast::FunctionStmt *>> extensions;
        error::ErrorHandler &errorHandler;
    };

    /**
     * @brief AST node for an extension function declaration
     *
     * Represents a function that extends an existing type, similar to
     * Kotlin's extension functions.
     */
    class ExtensionFunctionStmt : public ast::Statement
    {
    public:
        ExtensionFunctionStmt(const lexer::Token &token, const std::string &name,
                              std::vector<ast::Parameter> parameters,
                              ast::TypePtr returnType,
                              ast::StmtPtr body)
            : Statement(token), name(name), parameters(std::move(parameters)),
              returnType(returnType), body(body) {}

        virtual void accept(ast::Visitor &visitor) override
        {
            visitor.visitFunctionStmt(this);
        }

        ast::FunctionStmt *toRegularFunction()
        {
            lexer::Token token; // Create a default token
            ast::Parameter thisParam(token, "this", std::make_shared<ast::ClassType>(token, name));
            std::vector<ast::Parameter> allParams;
            allParams.push_back(thisParam);
            allParams.insert(allParams.end(), parameters.begin(), parameters.end());

            return new ast::FunctionStmt(token, name, allParams, returnType, body);
        }

    private:
        std::string name;
        std::vector<ast::Parameter> parameters;
        ast::TypePtr returnType;
        ast::StmtPtr body;
    };

    /**
     * @brief AST node for an extension call expression
     *
     * Represents a call to an extension function. This is transformed during
     * code generation into a call to the regular function with the target
     * object passed as the first argument.
     */
    class ExtensionCallExpr : public ast::Expression
    {
    public:
        ExtensionCallExpr(const lexer::Token &token, ast::ExprPtr target,
                         std::string functionName,
                         std::vector<ast::ExprPtr> arguments)
            : Expression(token), target(target), functionName(functionName),
              arguments(std::move(arguments)) {}

        virtual void accept(ast::Visitor &visitor) override
        {
            visitor.visitCallExpr(this);
        }

        virtual ast::TypePtr getType() const override
        {
            return nullptr; // This should be set during type checking
        }

        ast::CallExpr *toRegularCall()
        {
            lexer::Token token;  // Create a default token
            std::string fullName = target->getType()->toString() + "_" + functionName;
            auto funcExpr = std::make_shared<ast::VariableExpr>(token, fullName);
            std::vector<ast::ExprPtr> allArgs = {target};
            allArgs.insert(allArgs.end(), arguments.begin(), arguments.end());
            return new ast::CallExpr(token, funcExpr, allArgs);
        }

    private:
        ast::ExprPtr target;
        std::string functionName;
        std::vector<ast::ExprPtr> arguments;
    };

} // namespace type_checker
