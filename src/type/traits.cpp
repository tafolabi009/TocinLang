#include "traits.h"
#include "ast/ast.h"
#include "error/error_handler.h"
#include <algorithm>
#include <sstream>

namespace type_checker {

bool TraitManager::verifyImplementation(ast::TraitStmt* trait, ast::ImplStmt* impl) {
    if (!trait || !impl) {
        return false;
    }

    // Check if all required methods are implemented
    for (const auto& requiredMethod : trait->methods) {
        bool found = false;
        for (const auto& providedMethod : impl->methods) {
            if (requiredMethod->name.value == providedMethod->name.value) {
                if (verifyMethodSignature(requiredMethod.get(), providedMethod.get())) {
                    found = true;
                    break;
                }
            }
        }
        if (!found) {
            errorHandler_.error(impl->token, 
                "Missing or incorrect implementation of method '" + requiredMethod->name.value + 
                "' for trait '" + trait->name + "'");
            return false;
        }
    }

    return true;
}

bool TraitManager::typeImplementsTrait(const std::string& typeName, const std::string& traitName) const {
    auto it = impls_.find(traitName + "::" + typeName);
    return it != impls_.end();
}

bool TraitManager::verifyMethodSignature(ast::FunctionStmt* required, ast::FunctionStmt* provided) {
    if (!required || !provided) {
        return false;
    }

    // Check parameter count
    if (required->parameters.size() != provided->parameters.size()) {
        return false;
    }

    // Check parameter types
    for (size_t i = 0; i < required->parameters.size(); ++i) {
        if (required->parameters[i].type->toString() != provided->parameters[i].type->toString()) {
            return false;
        }
    }

    // Check return type
    if (required->returnType->toString() != provided->returnType->toString()) {
        return false;
    }

    return true;
}

} // namespace type_checker 