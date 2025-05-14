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
    ast::FunctionStmt* findExtension(ast::TypePtr targetType, const std::string &functionName)
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
    std::map<std::string, std::map<std::string, ast::FunctionStmt*>> extensions;
    error::ErrorHandler &errorHandler;
};

/**
 * @brief AST node for an extension function declaration
 * 
 * Represents a function that extends an existing type, similar to
 * Kotlin's extension functions.
 */
class ExtensionFunctionStmt : public ast::Stmt
{
public:
    ast::TypePtr targetType;      // The type being extended
    std::string name;             // Function name
    std::vector<ast::Parameter> parameters; // Function parameters (first is 'this')
    ast::TypePtr returnType;      // Return type
    ast::StmtPtr body;            // Function body
    
    ExtensionFunctionStmt(ast::TypePtr targetType, std::string name,
                         std::vector<ast::Parameter> parameters,
                         ast::TypePtr returnType, ast::StmtPtr body)
        : targetType(targetType), name(name), parameters(parameters),
          returnType(returnType), body(body) {}
    
    virtual void accept(ast::Visitor &visitor) override
    {
        // This would be implemented when we add the visitor method
        // visitor.visitExtensionFunctionStmt(this);
    }
    
    /**
     * @brief Convert this extension function to a regular function
     * 
     * This is used during code generation to transform the extension
     * function into a normal function with 'this' as the first parameter.
     * 
     * @return ast::FunctionStmt* The equivalent regular function
     */
    ast::FunctionStmt* toRegularFunction()
    {
        // Create a parameter for 'this'
        ast::Parameter thisParam;
        thisParam.name = "this";
        thisParam.type = targetType;
        
        // Copy other parameters
        std::vector<ast::Parameter> allParams = {thisParam};
        allParams.insert(allParams.end(), parameters.begin(), parameters.end());
        
        // Create a function with the name prefixed by the type
        std::string fullName = targetType->toString() + "_" + name;
        
        return new ast::FunctionStmt(
            fullName,
            allParams,
            returnType,
            body
        );
    }
};

/**
 * @brief AST node for an extension call expression
 * 
 * Represents a call to an extension function. This is transformed during
 * code generation into a call to the regular function with the target
 * object passed as the first argument.
 */
class ExtensionCallExpr : public ast::Expr
{
public:
    ast::ExprPtr target;                 // The target object (this)
    std::string name;                    // Extension function name
    std::vector<ast::ExprPtr> arguments; // Arguments (not including 'this')
    ast::FunctionStmt *extensionFunction; // The extension function being called
    
    ExtensionCallExpr(ast::ExprPtr target, std::string name,
                     std::vector<ast::ExprPtr> arguments,
                     ast::FunctionStmt *extensionFunction)
        : target(target), name(name), arguments(arguments),
          extensionFunction(extensionFunction) {}
    
    virtual void accept(ast::Visitor &visitor) override
    {
        // This would be implemented when we add the visitor method
        // visitor.visitExtensionCallExpr(this);
    }
    
    virtual ast::TypePtr getType() const override
    {
        // Return type is the extension function's return type
        if (extensionFunction)
        {
            return extensionFunction->returnType;
        }
        return nullptr;
    }
    
    /**
     * @brief Convert to a regular function call
     * 
     * Transform the extension call into a regular function call
     * with the target object as the first argument.
     * 
     * @return ast::CallExpr* The equivalent regular function call
     */
    ast::CallExpr* toRegularCall()
    {
        // Create the full name of the function
        std::string fullName = target->getType()->toString() + "_" + name;
        
        // Create a variable expression for the function
        auto funcExpr = std::make_shared<ast::VariableExpr>(fullName);
        
        // Create the arguments list with target as first argument
        std::vector<ast::ExprPtr> allArgs = {target};
        allArgs.insert(allArgs.end(), arguments.begin(), arguments.end());
        
        return new ast::CallExpr(funcExpr, allArgs);
    }
};

} // namespace type_checker 
