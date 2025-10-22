/**
 * @file type_system_enhanced.h
 * @brief Enhanced type system with complete validation and trait integration
 * 
 * This module provides comprehensive type checking, generic type instantiation,
 * and trait system integration for the Tocin compiler.
 */

#pragma once

#include "../ast/types.h"
#include "../util/result.h"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>

namespace tocin {
namespace type {

using util::Result;
using util::Option;
using util::CompilerError;

/**
 * @brief Type constraints for generic type parameters
 */
struct TypeConstraint {
    std::string traitName;
    std::vector<std::string> requiredMethods;
    
    TypeConstraint() = default;
    TypeConstraint(const std::string& trait) : traitName(trait) {}
};

/**
 * @brief Generic type parameter information
 */
struct TypeParameter {
    std::string name;
    std::vector<TypeConstraint> constraints;
    Option<ast::TypePtr> defaultType;
    
    TypeParameter() = default;
    TypeParameter(const std::string& n) : name(n) {}
};

/**
 * @brief Trait definition
 */
struct Trait {
    std::string name;
    std::vector<std::string> superTraits;
    std::vector<std::pair<std::string, ast::TypePtr>> methods;
    std::vector<std::pair<std::string, ast::TypePtr>> associatedTypes;
    
    Trait() = default;
    Trait(const std::string& n) : name(n) {}
};

/**
 * @brief Trait implementation for a type
 */
struct TraitImpl {
    std::string traitName;
    ast::TypePtr targetType;
    std::unordered_map<std::string, ast::ExprPtr> methodImpls;
    std::unordered_map<std::string, ast::TypePtr> associatedTypeImpls;
    
    TraitImpl() = default;
};

/**
 * @brief Enhanced type checker with validation and trait support
 */
class EnhancedTypeChecker {
public:
    EnhancedTypeChecker();
    ~EnhancedTypeChecker();
    
    // Type validation
    Result<ast::TypePtr, CompilerError> validateType(ast::TypePtr type);
    Result<bool, CompilerError> checkTypeCompatibility(ast::TypePtr from, ast::TypePtr to);
    Result<bool, CompilerError> isSubtype(ast::TypePtr sub, ast::TypePtr super);
    
    // Generic type instantiation
    Result<ast::TypePtr, CompilerError> instantiateGenericType(
        ast::TypePtr genericType,
        const std::vector<ast::TypePtr>& typeArgs
    );
    
    Result<bool, CompilerError> validateGenericInstantiation(
        ast::TypePtr genericType,
        const std::vector<ast::TypePtr>& typeArgs
    );
    
    // Trait system
    Result<bool, CompilerError> registerTrait(const Trait& trait);
    Result<bool, CompilerError> registerTraitImpl(const TraitImpl& impl);
    
    Result<bool, CompilerError> checkTraitConstraints(
        ast::TypePtr type,
        const std::vector<TypeConstraint>& constraints
    );
    
    Result<bool, CompilerError> doesTypeImplementTrait(
        ast::TypePtr type,
        const std::string& traitName
    );
    
    Option<Trait> getTrait(const std::string& name) const;
    Option<TraitImpl> getTraitImpl(const std::string& traitName, ast::TypePtr type) const;
    
    // Type inference
    Result<ast::TypePtr, CompilerError> inferType(ast::ExprPtr expr);
    Result<ast::TypePtr, CompilerError> unifyTypes(ast::TypePtr t1, ast::TypePtr t2);
    
    // Circular dependency detection
    Result<bool, CompilerError> checkCircularDependency(ast::TypePtr type);
    
    // Type information queries
    Option<size_t> getTypeSize(ast::TypePtr type) const;
    Option<size_t> getTypeAlignment(ast::TypePtr type) const;
    bool isNullable(ast::TypePtr type) const;
    bool isCopyable(ast::TypePtr type) const;
    bool isMovable(ast::TypePtr type) const;
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    
    // Helper methods
    bool typesEqual(ast::TypePtr t1, ast::TypePtr t2) const;
    ast::TypePtr substituteTypeParameters(
        ast::TypePtr type,
        const std::unordered_map<std::string, ast::TypePtr>& substitutions
    );
    
    Result<bool, CompilerError> validateMethodSignature(
        const std::string& methodName,
        ast::TypePtr signature,
        const Trait& trait
    );
};

/**
 * @brief Type registry for managing type information
 */
class TypeRegistry {
public:
    TypeRegistry();
    ~TypeRegistry();
    
    // Type registration
    Result<bool, CompilerError> registerType(const std::string& name, ast::TypePtr type);
    Result<bool, CompilerError> registerAlias(const std::string& alias, ast::TypePtr type);
    
    Option<ast::TypePtr> lookupType(const std::string& name) const;
    Option<ast::TypePtr> resolveAlias(const std::string& alias) const;
    
    // Generic type registration
    Result<bool, CompilerError> registerGenericType(
        const std::string& name,
        const std::vector<TypeParameter>& params,
        ast::TypePtr definition
    );
    
    Option<std::vector<TypeParameter>> getTypeParameters(const std::string& name) const;
    
    // Built-in types
    ast::TypePtr getInt32Type() const;
    ast::TypePtr getInt64Type() const;
    ast::TypePtr getFloat32Type() const;
    ast::TypePtr getFloat64Type() const;
    ast::TypePtr getBoolType() const;
    ast::TypePtr getStringType() const;
    ast::TypePtr getVoidType() const;
    ast::TypePtr getNullType() const;
    
    // Composite types
    ast::TypePtr makeArrayType(ast::TypePtr elementType);
    ast::TypePtr makePointerType(ast::TypePtr pointeeType);
    ast::TypePtr makeReferenceType(ast::TypePtr referentType);
    ast::TypePtr makeOptionType(ast::TypePtr innerType);
    ast::TypePtr makeResultType(ast::TypePtr okType, ast::TypePtr errType);
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/**
 * @brief Type constraint solver for generic type inference
 */
class ConstraintSolver {
public:
    ConstraintSolver();
    ~ConstraintSolver();
    
    struct Constraint {
        enum class Kind {
            Equality,        // T1 = T2
            Subtype,         // T1 <: T2
            TraitBound,      // T : Trait
            Associated       // T::Assoc = T2
        };
        
        Kind kind;
        ast::TypePtr lhs;
        ast::TypePtr rhs;
        std::string traitName;
        std::string associatedName;
        
        Constraint(Kind k) : kind(k) {}
    };
    
    void addConstraint(const Constraint& constraint);
    Result<std::unordered_map<std::string, ast::TypePtr>, CompilerError> solve();
    
    void clear();
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    
    Result<bool, CompilerError> unify(ast::TypePtr t1, ast::TypePtr t2);
    Result<bool, CompilerError> checkSubtype(ast::TypePtr sub, ast::TypePtr super);
};

/**
 * @brief Type conversion and coercion
 */
class TypeConverter {
public:
    TypeConverter();
    ~TypeConverter();
    
    // Implicit conversions
    bool canImplicitlyConvert(ast::TypePtr from, ast::TypePtr to) const;
    Result<ast::ExprPtr, CompilerError> implicitConvert(
        ast::ExprPtr expr,
        ast::TypePtr targetType
    );
    
    // Explicit conversions
    Result<ast::ExprPtr, CompilerError> explicitConvert(
        ast::ExprPtr expr,
        ast::TypePtr targetType
    );
    
    // Numeric conversions
    bool isNumericWidening(ast::TypePtr from, ast::TypePtr to) const;
    bool isNumericNarrowing(ast::TypePtr from, ast::TypePtr to) const;
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/**
 * @brief Type pretty printer
 */
class TypePrinter {
public:
    static std::string toString(ast::TypePtr type);
    static std::string toDebugString(ast::TypePtr type);
    static std::string toMangledName(ast::TypePtr type);
    
private:
    TypePrinter() = delete;
};

/**
 * @brief Type builder for constructing complex types programmatically
 */
class TypeBuilder {
public:
    TypeBuilder();
    ~TypeBuilder();
    
    TypeBuilder& setName(const std::string& name);
    TypeBuilder& addField(const std::string& name, ast::TypePtr type);
    TypeBuilder& addMethod(const std::string& name, ast::TypePtr returnType,
                          const std::vector<ast::TypePtr>& paramTypes);
    TypeBuilder& addTypeParameter(const TypeParameter& param);
    TypeBuilder& addTraitConstraint(const std::string& traitName);
    
    ast::TypePtr build();
    void reset();
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/**
 * @brief Utilities for working with types
 */
class TypeUtils {
public:
    // Type predicates
    static bool isIntegral(ast::TypePtr type);
    static bool isFloating(ast::TypePtr type);
    static bool isNumeric(ast::TypePtr type);
    static bool isSigned(ast::TypePtr type);
    static bool isPointer(ast::TypePtr type);
    static bool isReference(ast::TypePtr type);
    static bool isArray(ast::TypePtr type);
    static bool isFunction(ast::TypePtr type);
    static bool isGeneric(ast::TypePtr type);
    static bool isVoid(ast::TypePtr type);
    
    // Type decomposition
    static Option<ast::TypePtr> getElementType(ast::TypePtr arrayOrPointer);
    static Option<ast::TypePtr> getReturnType(ast::TypePtr functionType);
    static Option<std::vector<ast::TypePtr>> getParameterTypes(ast::TypePtr functionType);
    static Option<std::vector<ast::TypePtr>> getGenericArguments(ast::TypePtr genericType);
    
    // Type construction
    static ast::TypePtr makeQualified(ast::TypePtr base, bool isConst, bool isVolatile);
    static ast::TypePtr removeQualifiers(ast::TypePtr type);
    
    // Type comparison
    static bool areTypesEquivalent(ast::TypePtr t1, ast::TypePtr t2);
    static bool areTypesStructurallyEqual(ast::TypePtr t1, ast::TypePtr t2);
    
private:
    TypeUtils() = delete;
};

} // namespace type
} // namespace tocin
