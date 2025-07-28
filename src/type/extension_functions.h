#pragma once

#include "../ast/types.h"
#include "../ast/ast.h"
#include "../error/error_handler.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace type_checker {

/**
 * @brief Manages extension functions for types
 */
class ExtensionManager {
public:
    explicit ExtensionManager(error::ErrorHandler& errorHandler)
        : errorHandler_(errorHandler) {}

    /**
     * @brief Register an extension function for a type
     */
    void registerExtension(const std::string& typeName, 
                          const std::string& functionName,
                          std::shared_ptr<ast::FunctionStmt> function);

    /**
     * @brief Find extension functions for a type
     */
    std::vector<std::shared_ptr<ast::FunctionStmt>> 
    getExtensions(const std::string& typeName);

    /**
     * @brief Check if a type has a specific extension function
     */
    bool hasExtension(const std::string& typeName, const std::string& functionName);

    /**
     * @brief Get a specific extension function
     */
    std::shared_ptr<ast::FunctionStmt> 
    getExtension(const std::string& typeName, const std::string& functionName);

    /**
     * @brief Validate that an extension function is properly defined
     */
    bool validateExtension(const std::string& typeName, 
                          std::shared_ptr<ast::FunctionStmt> function);

private:
    error::ErrorHandler& errorHandler_;
    
    // Map from type name to map of function name to function
    std::unordered_map<std::string, 
                      std::unordered_map<std::string, 
                                        std::shared_ptr<ast::FunctionStmt>>> extensions_;
};

} // namespace type_checker
