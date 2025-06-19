#pragma once

#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include "ast/ast.h"
#include "ast/stmt.h"
#include "ast/expr.h"
#include "ast/types.h"
#include "error/error_handler.h"

namespace type_checker {

// Forward declarations
class TraitStmt;
class ImplStmt;
class TraitType;
class DynTraitType;

/**
 * @brief Manages trait definitions and implementations
 */
class TraitManager {
public:
    explicit TraitManager(error::ErrorHandler& errorHandler) 
        : errorHandler_(errorHandler) {}

    /**
     * @brief Register a trait definition
     */
    void registerTrait(const std::string& name, std::shared_ptr<ast::TraitStmt> trait) {
        traits_[name] = trait;
    }

    /**
     * @brief Get a trait by name
     */
    std::shared_ptr<ast::TraitStmt> getTrait(const std::string& name) const {
        auto it = traits_.find(name);
        return (it != traits_.end()) ? it->second : nullptr;
    }

    /**
     * @brief Register an implementation
     */
    void registerImplementation(const std::string& traitName, 
                              const std::string& typeName,
                              std::shared_ptr<ast::ImplStmt> impl) {
        impls_[traitName + "::" + typeName] = impl;
    }

    /**
     * @brief Get an implementation
     */
    std::shared_ptr<ast::ImplStmt> getImplementation(const std::string& traitName,
                                               const std::string& typeName) const {
        auto it = impls_.find(traitName + "::" + typeName);
        return (it != impls_.end()) ? it->second : nullptr;
    }

    /**
     * @brief Verify that an implementation satisfies a trait
     */
    bool verifyImplementation(ast::TraitStmt* trait, ast::ImplStmt* impl);

    /**
     * @brief Check if a type implements a trait
     */
    bool typeImplementsTrait(const std::string& typeName, const std::string& traitName) const;

private:
    error::ErrorHandler& errorHandler_;
    std::unordered_map<std::string, std::shared_ptr<ast::TraitStmt>> traits_;
    std::unordered_map<std::string, std::shared_ptr<ast::ImplStmt>> impls_;

    bool verifyMethodSignature(ast::FunctionStmt* required, ast::FunctionStmt* provided);
};

/**
 * @brief Trait type - represents a trait as a type
 */
class TraitType : public ast::Type {
public:
    std::shared_ptr<ast::TraitStmt> traitStmt;

    TraitType(const lexer::Token& token, std::shared_ptr<ast::TraitStmt> trait)
        : ast::Type(token), traitStmt(trait) {}

    std::string toString() const override {
        return "trait " + traitStmt->name;
    }

    std::shared_ptr<ast::TraitStmt> getTraitStmt() const {
        return traitStmt;
    }
};

/**
 * @brief Dynamic trait type - for trait objects
 */
class DynTraitType : public ast::Type {
public:
    ast::TypePtr underlyingType;

    DynTraitType(const lexer::Token& token, ast::TypePtr type)
        : ast::Type(token), underlyingType(type) {}

    std::string toString() const override {
        return "dyn " + underlyingType->toString();
    }

    ast::TypePtr clone() const override {
        return std::make_shared<DynTraitType>(token, underlyingType->clone());
    }
};

} // namespace type_checker 