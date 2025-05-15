#pragma once

#include "../pch.h"
#include "../ast/ast.h"
#include "../error/error_handler.h"

namespace type_checker
{

    /**
     * @brief AST node for a trait declaration
     *
     * Traits define a set of methods that a type must implement,
     * similar to Rust traits or interfaces in other languages.
     */
    class TraitStmt : public ast::ClassStmt
    {
    public:
        TraitStmt(const lexer::Token &token, const std::string &name,
                  std::vector<ast::TypeParameter> typeParams,
                  std::vector<ast::FunctionStmt *> methods)
            : ast::ClassStmt(token, name, typeParams, nullptr, {}, {}, {}),
              methods(std::move(methods)) {}

        virtual void accept(ast::Visitor &visitor) override
        {
            visitor.visitClassStmt(this);
        }

        const std::string &getName() const { return name; }
        const std::vector<ast::TypeParameter> &getTypeParams() const { return typeParameters; }
        const std::vector<ast::FunctionStmt *> &getMethods() const { return methods; }
        const std::vector<ast::TypePtr> &getSuperTraits() const { return superTraits; }

    private:
        std::vector<ast::FunctionStmt *> methods;
        std::vector<ast::TypePtr> superTraits;
    };

    /**
     * @brief AST node for implementing a trait for a type
     *
     * This is similar to Rust's impl Trait for Type, allowing a type
     * to implement a specific trait interface.
     */
    class ImplStmt : public ast::ClassStmt
    {
    public:
        ImplStmt(const lexer::Token &token, ast::TypePtr type,
                 std::vector<ast::TypePtr> traits,
                 std::vector<ast::FunctionStmt *> methods)
            : ast::ClassStmt(token, type->toString(), {}, nullptr, traits, {}, {}),
              type(type),
              implMethods(std::move(methods)) {}

        virtual void accept(ast::Visitor &visitor) override
        {
            visitor.visitClassStmt(this);
        }

        const ast::TypePtr &getType() const { return type; }
        const std::vector<ast::TypePtr> &getTraits() const { return interfaces; }
        const std::vector<ast::FunctionStmt *> &getMethods() const { return implMethods; }

    private:
        ast::TypePtr type;
        std::vector<ast::FunctionStmt *> implMethods;
    };

    /**
     * @brief AST node for a trait type bound
     *
     * Used in generic function/type declarations to constrain type parameters
     * to types that implement specific traits, like T: Display + Clone in Rust.
     */
    class TraitBound : public ast::Type
    {
    public:
        TraitBound(const lexer::Token &token, const std::string &typeParameter,
                   const std::vector<ast::TypePtr> &requiredTraits)
            : Type(token), typeParameter(typeParameter), requiredTraits(requiredTraits) {}

        const std::string &getTypeParameter() const { return typeParameter; }
        const std::vector<ast::TypePtr> &getRequiredTraits() const { return requiredTraits; }

        virtual std::string toString() const override
        {
            std::string result = typeParameter + ": ";
            for (size_t i = 0; i < requiredTraits.size(); ++i)
            {
                if (i > 0)
                    result += " + ";
                result += requiredTraits[i]->toString();
            }
            return result;
        }
        
        ast::TypePtr clone() const override
        {
            std::vector<ast::TypePtr> clonedTraits;
            for (const auto &trait : requiredTraits)
            {
                clonedTraits.push_back(trait->clone());
            }
            return std::make_shared<TraitBound>(token, typeParameter, clonedTraits);
        }

    private:
        std::string typeParameter;
        std::vector<ast::TypePtr> requiredTraits;
    };

    /**
     * @brief Manager for trait implementations and trait resolution
     *
     * Handles checking trait implementations, type bounds, and method resolution.
     */
    class TraitManager
    {
    public:
        TraitManager(error::ErrorHandler &errorHandler)
            : errorHandler(errorHandler) {}

        /**
         * @brief Register a trait declaration
         *
         * @param trait The trait being registered
         * @return true if registration succeeded
         */
        bool registerTrait(TraitStmt *trait)
        {
            // Check if trait already registered
            if (traits.find(trait->getName()) != traits.end())
            {
                errorHandler.reportError(
                    error::ErrorCode::T004_UNDEFINED_TYPE,
                    "Trait '" + trait->getName() + "' already defined",
                    "", 0, 0, error::ErrorSeverity::ERROR);
                return false;
            }

            // Register the trait
            traits[trait->getName()] = trait;

            return true;
        }

        /**
         * @brief Register a trait implementation for a type
         *
         * @param impl The implementation statement
         * @return true if registration succeeded
         */
        bool registerImpl(ImplStmt *impl)
        {
            // Get trait and type names
            std::string traitName = impl->getType()->toString();
            std::string typeName = impl->getType()->toString();

            // Check if trait exists
            auto traitIt = traits.find(traitName);
            if (traitIt == traits.end())
            {
                errorHandler.reportError(
                    error::ErrorCode::T004_UNDEFINED_TYPE,
                    "Cannot implement undefined trait '" + traitName + "'",
                    "", 0, 0, error::ErrorSeverity::ERROR);
                return false;
            }

            // Check if implementation already exists
            auto &implsForType = implementations[typeName];
            if (implsForType.find(traitName) != implsForType.end())
            {
                errorHandler.reportError(
                    error::ErrorCode::T001_TYPE_MISMATCH,
                    "Type '" + typeName + "' already implements trait '" + traitName + "'",
                    "", 0, 0, error::ErrorSeverity::ERROR);
                return false;
            }

            // Verify that all required methods are implemented
            TraitStmt *trait = traitIt->second;
            if (!verifyImplementation(trait, impl))
            {
                return false;
            }

            // Register the implementation
            implsForType[traitName] = impl;

            return true;
        }

        /**
         * @brief Check if a type implements a trait
         *
         * @param type The type to check
         * @param traitType The trait to check for
         * @return true if the type implements the trait
         */
        bool doesTypeImplementTrait(ast::TypePtr type, ast::TypePtr traitType)
        {
            std::string typeName = type->toString();
            std::string traitName = traitType->toString();

            // Check direct implementation
            auto typeIt = implementations.find(typeName);
            if (typeIt != implementations.end())
            {
                if (typeIt->second.find(traitName) != typeIt->second.end())
                {
                    return true;
                }
            }

            // Check super traits
            auto traitIt = traits.find(traitName);
            if (traitIt != traits.end())
            {
                TraitStmt *trait = traitIt->second;
                for (const auto &superTrait : trait->getSuperTraits())
                {
                    if (doesTypeImplementTrait(type, superTrait))
                    {
                        return true;
                    }
                }
            }

            return false;
        }

        /**
         * @brief Find the implementation of a trait method for a type
         *
         * @param type The type
         * @param traitType The trait
         * @param methodName The method name
         * @return ast::FunctionStmt* The method implementation, or nullptr if not found
         */
        ast::FunctionStmt *findTraitMethod(ast::TypePtr type, ast::TypePtr traitType, const std::string &methodName)
        {
            std::string typeName = type->toString();
            std::string traitName = traitType->toString();

            // Check if the type implements the trait
            auto typeIt = implementations.find(typeName);
            if (typeIt != implementations.end())
            {
                auto implIt = typeIt->second.find(traitName);
                if (implIt != typeIt->second.end())
                {
                    ImplStmt *impl = implIt->second;

                    // Find the method in the implementation
                    for (ast::FunctionStmt *method : impl->getMethods())
                    {
                        if (method->name == methodName)
                        {
                            return method;
                        }
                    }
                }
            }

            // If not found, check for default implementations in the trait
            auto traitIt = traits.find(traitName);
            if (traitIt != traits.end())
            {
                TraitStmt *trait = traitIt->second;

                // Check for default implementations
                for (ast::FunctionStmt *method : trait->getMethods())
                {
                    if (method->name == methodName && method->body != nullptr)
                    {
                        return method;
                    }
                }

                // Check super traits
                for (const auto &superTrait : trait->getSuperTraits())
                {
                    ast::FunctionStmt *method = findTraitMethod(type, superTrait, methodName);
                    if (method != nullptr)
                    {
                        return method;
                    }
                }
            }

            return nullptr;
        }

        /**
         * @brief Check if a type satisfies a set of trait bounds
         *
         * @param type The type to check
         * @param bounds The trait bounds to satisfy
         * @return true if the type satisfies all bounds
         */
        bool checkTraitBounds(ast::TypePtr type, const std::vector<ast::TypePtr> &bounds)
        {
            for (const auto &bound : bounds)
            {
                if (auto traitBound = std::dynamic_pointer_cast<TraitBound>(bound))
                {
                    // For type parameters, store the bound for later checking
                    // This is a simplified approach

                    // For concrete types, check each required trait
                    for (const auto &requiredTrait : traitBound->getRequiredTraits())
                    {
                        if (!doesTypeImplementTrait(type, requiredTrait))
                        {
                            errorHandler.reportError(
                                error::ErrorCode::T001_TYPE_MISMATCH,
                                "Type '" + type->toString() + "' does not implement required trait '" +
                                    requiredTrait->toString() + "'",
                                "", 0, 0, error::ErrorSeverity::ERROR);
                            return false;
                        }
                    }
                }
            }

            return true;
        }

    private:
        // Map of trait name -> trait declaration
        std::map<std::string, TraitStmt *> traits;

        // Map of type name -> (trait name -> implementation)
        std::map<std::string, std::map<std::string, ImplStmt *>> implementations;

        error::ErrorHandler &errorHandler;

        /**
         * @brief Verify that an implementation satisfies all trait requirements
         *
         * @param trait The trait being implemented
         * @param impl The implementation
         * @return true if the implementation is valid
         */
        bool verifyImplementation(TraitStmt *trait, ImplStmt *impl)
        {
            // Check that all required methods are implemented
            for (ast::FunctionStmt *requiredMethod : trait->getMethods())
            {
                // Skip methods with default implementations
                if (requiredMethod->body != nullptr)
                {
                    continue;
                }

                // Find the implementation of this method
                bool found = false;
                for (ast::FunctionStmt *implMethod : impl->getMethods())
                {
                    if (implMethod->name == requiredMethod->name)
                    {
                        // Verify method signature matches
                        if (!verifyMethodSignature(requiredMethod, implMethod))
                        {
                            errorHandler.reportError(
                                error::ErrorCode::T003_UNDEFINED_FUNCTION,
                                "Method '" + requiredMethod->name + "' implementation doesn't match trait signature",
                                "", 0, 0, error::ErrorSeverity::ERROR);
                            return false;
                        }

                        found = true;
                        break;
                    }
                }

                if (!found)
                {
                    errorHandler.reportError(
                        error::ErrorCode::T003_UNDEFINED_FUNCTION,
                        "Missing implementation for required method '" + requiredMethod->name + "'",
                        "", 0, 0, error::ErrorSeverity::ERROR);
                    return false;
                }
            }

            return true;
        }

        /**
         * @brief Verify that a method implementation matches the trait signature
         *
         * @param traitMethod The method signature in the trait
         * @param implMethod The method implementation
         * @return true if the signatures match
         */
        bool verifyMethodSignature(ast::FunctionStmt *traitMethod, ast::FunctionStmt *implMethod)
        {
            // Check return type
            if (traitMethod->returnType->toString() != implMethod->returnType->toString())
            {
                return false;
            }

            // Check parameter count and types
            if (traitMethod->parameters.size() != implMethod->parameters.size())
            {
                return false;
            }

            for (size_t i = 0; i < traitMethod->parameters.size(); ++i)
            {
                if (traitMethod->parameters[i].type->toString() !=
                    implMethod->parameters[i].type->toString())
                {
                    return false;
                }
            }

            return true;
        }
    };

    /**
     * @brief AST node for a dynamic trait object
     *
     * Similar to Rust's dyn Trait, this represents a value of any type that
     * implements the specified trait, using dynamic dispatch.
     */
    class DynTraitType : public ast::Type
    {
    public:
        DynTraitType(const lexer::Token &token, ast::TypePtr traitType)
            : Type(token), traitType(traitType) {}

        virtual std::string toString() const override
        {
            return "dyn " + traitType->toString();
        }

        const ast::TypePtr &getTraitType() const { return traitType; }

        ast::TypePtr clone() const override
        {
            return std::make_shared<DynTraitType>(token, traitType->clone());
        }

    private:
        ast::TypePtr traitType;
    };

    /**
     * @brief AST node for a trait method call
     *
     * Represents a call to a method through a trait interface,
     * which may use either static or dynamic dispatch.
     */
    class TraitCallExpr : public ast::CallExpr
    {
    public:
        TraitCallExpr(const lexer::Token &token, ast::ExprPtr target,
                      ast::TypePtr traitType,
                      const std::string &methodName,
                      std::vector<ast::ExprPtr> arguments)
            : CallExpr(token, target, arguments), traitType(traitType),
              methodName(methodName) {}

        virtual void accept(ast::Visitor &visitor) override
        {
            visitor.visitCallExpr(this);
        }

        virtual ast::TypePtr getType() const override
        {
            return nullptr; // This should be set during type checking
        }

        const ast::TypePtr &getTraitType() const { return traitType; }
        const std::string &getMethodName() const { return methodName; }

    private:
        ast::TypePtr traitType;
        std::string methodName;
    };

} // namespace type_checker
