#pragma once

#include "../ast/ast.h"
#include "../error/error_handler.h"
#include "../lexer/token.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>

namespace compiler {

/**
 * @brief Macro expansion context
 */
struct MacroContext {
    std::string macroName;
    std::vector<ast::ExprPtr> arguments;
    std::unordered_map<std::string, ast::ExprPtr> capturedVars;
    int expansionDepth;
    std::string currentFile;
    int currentLine;
    
    MacroContext() : expansionDepth(0), currentLine(0) {}
};

/**
 * @brief Macro definition interface
 */
class MacroDefinition {
public:
    virtual ~MacroDefinition() = default;
    
    /**
     * @brief Expand the macro with given arguments
     */
    virtual ast::StmtPtr expand(const MacroContext& context, error::ErrorHandler& errorHandler) = 0;
    
    /**
     * @brief Get macro name
     */
    virtual std::string getName() const = 0;
    
    /**
     * @brief Get parameter count
     */
    virtual size_t getParameterCount() const = 0;
    
    /**
     * @brief Check if macro is variadic
     */
    virtual bool isVariadic() const = 0;
};

/**
 * @brief Function-like macro definition
 */
class FunctionMacro : public MacroDefinition {
private:
    std::string name;
    std::vector<std::string> parameters;
    ast::StmtPtr body;
    bool variadic;
    
public:
    FunctionMacro(const std::string& name, const std::vector<std::string>& params, 
                  ast::StmtPtr body, bool variadic = false)
        : name(name), parameters(params), body(std::move(body)), variadic(variadic) {}
    
    ast::StmtPtr expand(const MacroContext& context, error::ErrorHandler& errorHandler) override;
    std::string getName() const override { return name; }
    size_t getParameterCount() const override { return parameters.size(); }
    bool isVariadic() const override { return variadic; }
};

/**
 * @brief Procedural macro definition
 */
class ProceduralMacro : public MacroDefinition {
private:
    std::string name;
    std::function<ast::StmtPtr(const MacroContext&, error::ErrorHandler&)> expander;
    
public:
    ProceduralMacro(const std::string& name, 
                    std::function<ast::StmtPtr(const MacroContext&, error::ErrorHandler&)> exp)
        : name(name), expander(std::move(exp)) {}
    
    ast::StmtPtr expand(const MacroContext& context, error::ErrorHandler& errorHandler) override {
        return expander(context, errorHandler);
    }
    
    std::string getName() const override { return name; }
    size_t getParameterCount() const override { return 0; } // Procedural macros don't have fixed parameters
    bool isVariadic() const override { return true; }
};

/**
 * @brief Macro system manager
 */
class MacroSystem {
private:
    std::unordered_map<std::string, std::unique_ptr<MacroDefinition>> macros;
    std::unordered_map<std::string, std::string> builtinMacros;
    int maxExpansionDepth;
    
public:
    MacroSystem() : maxExpansionDepth(100) {
        registerBuiltinMacros();
    }
    
    /**
     * @brief Register a macro definition
     */
    void registerMacro(std::unique_ptr<MacroDefinition> macro);
    
    /**
     * @brief Expand a macro call
     */
    ast::StmtPtr expandMacro(const std::string& macroName, const MacroContext& context, 
                             error::ErrorHandler& errorHandler);
    
    /**
     * @brief Check if a macro exists
     */
    bool hasMacro(const std::string& name) const;
    
    /**
     * @brief Get macro definition
     */
    MacroDefinition* getMacro(const std::string& name);
    
    /**
     * @brief Parse macro definition from AST
     */
    std::unique_ptr<MacroDefinition> parseMacroDefinition(ast::StmtPtr stmt, 
                                                         error::ErrorHandler& errorHandler);
    
    /**
     * @brief Process macros in a statement
     */
    ast::StmtPtr processMacros(ast::StmtPtr stmt, error::ErrorHandler& errorHandler);
    
    /**
     * @brief Process macros in an expression
     */
    ast::ExprPtr processMacros(ast::ExprPtr expr, error::ErrorHandler& errorHandler);
    
private:
    void registerBuiltinMacros();
    ast::StmtPtr substituteMacroArguments(ast::StmtPtr stmt, const MacroContext& context);
    ast::ExprPtr substituteMacroArguments(ast::ExprPtr expr, const MacroContext& context);
    std::string generateUniqueIdentifier(const std::string& base);
};

/**
 * @brief Built-in macros
 */
namespace builtin_macros {
    
    // Debug macro
    ast::StmtPtr debugMacro(const MacroContext& context, error::ErrorHandler& errorHandler);
    
    // Assert macro
    ast::StmtPtr assertMacro(const MacroContext& context, error::ErrorHandler& errorHandler);
    
    // Measure macro
    ast::StmtPtr measureMacro(const MacroContext& context, error::ErrorHandler& errorHandler);
    
    // Repeat macro
    ast::StmtPtr repeatMacro(const MacroContext& context, error::ErrorHandler& errorHandler);
    
    // If macro
    ast::StmtPtr ifMacro(const MacroContext& context, error::ErrorHandler& errorHandler);
    
    // Match macro
    ast::StmtPtr matchMacro(const MacroContext& context, error::ErrorHandler& errorHandler);
    
    // For macro
    ast::StmtPtr forMacro(const MacroContext& context, error::ErrorHandler& errorHandler);
    
    // Let macro
    ast::StmtPtr letMacro(const MacroContext& context, error::ErrorHandler& errorHandler);
    
    // Try macro
    ast::StmtPtr tryMacro(const MacroContext& context, error::ErrorHandler& errorHandler);
    
    // Log macro
    ast::StmtPtr logMacro(const MacroContext& context, error::ErrorHandler& errorHandler);
    
    // Profile macro
    ast::StmtPtr profileMacro(const MacroContext& context, error::ErrorHandler& errorHandler);
}

} // namespace compiler 