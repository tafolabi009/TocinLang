/**
 * @file type_system_enhanced.cpp
 * @brief Implementation of enhanced type system
 */

#include "type_system_enhanced.h"
#include <algorithm>
#include <sstream>

namespace tocin {
namespace type {

// ============================================================================
// EnhancedTypeChecker Implementation
// ============================================================================

struct EnhancedTypeChecker::Impl {
    std::unordered_map<std::string, Trait> traits;
    std::vector<TraitImpl> traitImpls;
    std::unordered_set<std::string> visitedTypes; // For circular dependency detection
    
    TypeRegistry registry;
};

EnhancedTypeChecker::EnhancedTypeChecker() : impl_(std::make_unique<Impl>()) {}

EnhancedTypeChecker::~EnhancedTypeChecker() = default;

Result<ast::TypePtr, CompilerError> EnhancedTypeChecker::validateType(ast::TypePtr type) {
    if (!type) {
        return Result<ast::TypePtr, CompilerError>::Err(
            CompilerError("Null type pointer"));
    }
    
    // Check for circular dependencies
    auto circularCheck = checkCircularDependency(type);
    if (circularCheck.isErr()) {
        return Result<ast::TypePtr, CompilerError>::Err(circularCheck.unwrapErr());
    }
    
    // Validate based on type kind
    if (auto simpleType = std::dynamic_pointer_cast<ast::SimpleType>(type)) {
        auto resolved = impl_->registry.lookupType(simpleType->name);
        if (resolved.isNone()) {
            return Result<ast::TypePtr, CompilerError>::Err(
                CompilerError("Unknown type: " + simpleType->name));
        }
        return Result<ast::TypePtr, CompilerError>::Ok(type);
    }
    
    if (auto genericType = std::dynamic_pointer_cast<ast::GenericType>(type)) {
        // Validate all type arguments
        for (const auto& arg : genericType->typeArgs) {
            auto result = validateType(arg);
            if (result.isErr()) {
                return result;
            }
        }
        
        // Check generic instantiation validity
        auto validation = validateGenericInstantiation(type, genericType->typeArgs);
        if (validation.isErr()) {
            return Result<ast::TypePtr, CompilerError>::Err(validation.unwrapErr());
        }
        
        return Result<ast::TypePtr, CompilerError>::Ok(type);
    }
    
    return Result<ast::TypePtr, CompilerError>::Ok(type);
}

Result<bool, CompilerError> EnhancedTypeChecker::checkTypeCompatibility(
    ast::TypePtr from, ast::TypePtr to) {
    
    if (!from || !to) {
        return Result<bool, CompilerError>::Err(
            CompilerError("Null type in compatibility check"));
    }
    
    // Exact match
    if (typesEqual(from, to)) {
        return Result<bool, CompilerError>::Ok(true);
    }
    
    // Check subtyping relationship
    return isSubtype(from, to);
}

Result<bool, CompilerError> EnhancedTypeChecker::isSubtype(
    ast::TypePtr sub, ast::TypePtr super) {
    
    // Null type is subtype of all pointer types
    if (auto nullType = std::dynamic_pointer_cast<ast::SimpleType>(sub)) {
        if (nullType->name == "null" && std::dynamic_pointer_cast<ast::PointerType>(super)) {
            return Result<bool, CompilerError>::Ok(true);
        }
    }
    
    // TODO: Implement full subtyping rules
    // - Class inheritance
    // - Trait implementation
    // - Variance for generic types
    
    return Result<bool, CompilerError>::Ok(false);
}

Result<ast::TypePtr, CompilerError> EnhancedTypeChecker::instantiateGenericType(
    ast::TypePtr genericType,
    const std::vector<ast::TypePtr>& typeArgs) {
    
    auto validation = validateGenericInstantiation(genericType, typeArgs);
    if (validation.isErr()) {
        return Result<ast::TypePtr, CompilerError>::Err(validation.unwrapErr());
    }
    
    // Create substitution map
    auto genType = std::dynamic_pointer_cast<ast::GenericType>(genericType);
    if (!genType) {
        return Result<ast::TypePtr, CompilerError>::Err(
            CompilerError("Expected generic type"));
    }
    
    std::unordered_map<std::string, ast::TypePtr> substitutions;
    auto params = impl_->registry.getTypeParameters(genType->name);
    
    if (params.isSome() && params.unwrap().size() == typeArgs.size()) {
        for (size_t i = 0; i < typeArgs.size(); ++i) {
            substitutions[params.unwrap()[i].name] = typeArgs[i];
        }
    }
    
    // Perform substitution
    auto instantiated = substituteTypeParameters(genericType, substitutions);
    return Result<ast::TypePtr, CompilerError>::Ok(instantiated);
}

Result<bool, CompilerError> EnhancedTypeChecker::validateGenericInstantiation(
    ast::TypePtr genericType,
    const std::vector<ast::TypePtr>& typeArgs) {
    
    auto genType = std::dynamic_pointer_cast<ast::GenericType>(genericType);
    if (!genType) {
        return Result<bool, CompilerError>::Err(
            CompilerError("Not a generic type"));
    }
    
    auto params = impl_->registry.getTypeParameters(genType->name);
    if (params.isNone()) {
        return Result<bool, CompilerError>::Err(
            CompilerError("Type parameters not found for: " + genType->name));
    }
    
    if (params.unwrap().size() != typeArgs.size()) {
        return Result<bool, CompilerError>::Err(
            CompilerError("Wrong number of type arguments"));
    }
    
    // Check constraints for each type argument
    for (size_t i = 0; i < typeArgs.size(); ++i) {
        const auto& param = params.unwrap()[i];
        const auto& arg = typeArgs[i];
        
        auto constraintCheck = checkTraitConstraints(arg, param.constraints);
        if (constraintCheck.isErr()) {
            return constraintCheck;
        }
    }
    
    return Result<bool, CompilerError>::Ok(true);
}

Result<bool, CompilerError> EnhancedTypeChecker::registerTrait(const Trait& trait) {
    if (impl_->traits.count(trait.name)) {
        return Result<bool, CompilerError>::Err(
            CompilerError("Trait already registered: " + trait.name));
    }
    
    impl_->traits[trait.name] = trait;
    return Result<bool, CompilerError>::Ok(true);
}

Result<bool, CompilerError> EnhancedTypeChecker::registerTraitImpl(const TraitImpl& impl) {
    // Validate that trait exists
    if (!impl_->traits.count(impl.traitName)) {
        return Result<bool, CompilerError>::Err(
            CompilerError("Unknown trait: " + impl.traitName));
    }
    
    const auto& trait = impl_->traits[impl.traitName];
    
    // Validate all required methods are implemented
    for (const auto& [methodName, methodType] : trait.methods) {
        if (!impl.methodImpls.count(methodName)) {
            return Result<bool, CompilerError>::Err(
                CompilerError("Missing method implementation: " + methodName));
        }
        
        // Validate method signature
        auto validation = validateMethodSignature(methodName, methodType, trait);
        if (validation.isErr()) {
            return validation;
        }
    }
    
    impl_->traitImpls.push_back(impl);
    return Result<bool, CompilerError>::Ok(true);
}

Result<bool, CompilerError> EnhancedTypeChecker::checkTraitConstraints(
    ast::TypePtr type,
    const std::vector<TypeConstraint>& constraints) {
    
    for (const auto& constraint : constraints) {
        auto result = doesTypeImplementTrait(type, constraint.traitName);
        if (result.isErr() || !result.unwrap()) {
            return Result<bool, CompilerError>::Err(
                CompilerError("Type does not satisfy trait constraint: " + constraint.traitName));
        }
    }
    
    return Result<bool, CompilerError>::Ok(true);
}

Result<bool, CompilerError> EnhancedTypeChecker::doesTypeImplementTrait(
    ast::TypePtr type,
    const std::string& traitName) {
    
    if (!impl_->traits.count(traitName)) {
        return Result<bool, CompilerError>::Err(
            CompilerError("Unknown trait: " + traitName));
    }
    
    // Search for trait implementation
    for (const auto& impl : impl_->traitImpls) {
        if (impl.traitName == traitName && typesEqual(impl.targetType, type)) {
            return Result<bool, CompilerError>::Ok(true);
        }
    }
    
    return Result<bool, CompilerError>::Ok(false);
}

Option<Trait> EnhancedTypeChecker::getTrait(const std::string& name) const {
    auto it = impl_->traits.find(name);
    if (it != impl_->traits.end()) {
        return Option<Trait>::Some(it->second);
    }
    return Option<Trait>::None();
}

Option<TraitImpl> EnhancedTypeChecker::getTraitImpl(
    const std::string& traitName, ast::TypePtr type) const {
    
    for (const auto& impl : impl_->traitImpls) {
        if (impl.traitName == traitName && typesEqual(impl.targetType, type)) {
            return Option<TraitImpl>::Some(impl);
        }
    }
    return Option<TraitImpl>::None();
}

Result<ast::TypePtr, CompilerError> EnhancedTypeChecker::inferType(ast::ExprPtr expr) {
    // TODO: Implement full type inference
    // For now, return a placeholder
    return Result<ast::TypePtr, CompilerError>::Ok(impl_->registry.getVoidType());
}

Result<ast::TypePtr, CompilerError> EnhancedTypeChecker::unifyTypes(
    ast::TypePtr t1, ast::TypePtr t2) {
    
    if (typesEqual(t1, t2)) {
        return Result<ast::TypePtr, CompilerError>::Ok(t1);
    }
    
    // TODO: Implement unification algorithm
    return Result<ast::TypePtr, CompilerError>::Err(
        CompilerError("Cannot unify types"));
}

Result<bool, CompilerError> EnhancedTypeChecker::checkCircularDependency(ast::TypePtr type) {
    // TODO: Implement cycle detection in type definitions
    return Result<bool, CompilerError>::Ok(false);
}

Option<size_t> EnhancedTypeChecker::getTypeSize(ast::TypePtr type) const {
    // TODO: Calculate type size based on target architecture
    return Option<size_t>::None();
}

Option<size_t> EnhancedTypeChecker::getTypeAlignment(ast::TypePtr type) const {
    // TODO: Calculate type alignment
    return Option<size_t>::None();
}

bool EnhancedTypeChecker::isNullable(ast::TypePtr type) const {
    // Pointer types and Option types are nullable
    return std::dynamic_pointer_cast<ast::PointerType>(type) != nullptr;
}

bool EnhancedTypeChecker::isCopyable(ast::TypePtr type) const {
    // By default, assume types are copyable
    // TODO: Check for move-only semantics
    return true;
}

bool EnhancedTypeChecker::isMovable(ast::TypePtr type) const {
    // All types are movable by default
    return true;
}

bool EnhancedTypeChecker::typesEqual(ast::TypePtr t1, ast::TypePtr t2) const {
    if (!t1 && !t2) return true;
    if (!t1 || !t2) return false;
    
    // TODO: Implement deep equality check
    return TypePrinter::toString(t1) == TypePrinter::toString(t2);
}

ast::TypePtr EnhancedTypeChecker::substituteTypeParameters(
    ast::TypePtr type,
    const std::unordered_map<std::string, ast::TypePtr>& substitutions) {
    
    // TODO: Implement type parameter substitution
    return type;
}

Result<bool, CompilerError> EnhancedTypeChecker::validateMethodSignature(
    const std::string& methodName,
    ast::TypePtr signature,
    const Trait& trait) {
    
    // TODO: Implement method signature validation
    return Result<bool, CompilerError>::Ok(true);
}

// ============================================================================
// TypeRegistry Implementation
// ============================================================================

struct TypeRegistry::Impl {
    std::unordered_map<std::string, ast::TypePtr> types;
    std::unordered_map<std::string, ast::TypePtr> aliases;
    std::unordered_map<std::string, std::vector<TypeParameter>> genericTypes;
};

TypeRegistry::TypeRegistry() : impl_(std::make_unique<Impl>()) {
    // Register built-in types
    impl_->types["int"] = std::make_shared<ast::SimpleType>(
        lexer::Token(lexer::TokenType::IDENTIFIER, "int", "", 0, 0));
    impl_->types["int32"] = impl_->types["int"];
    impl_->types["int64"] = std::make_shared<ast::SimpleType>(
        lexer::Token(lexer::TokenType::IDENTIFIER, "int64", "", 0, 0));
    impl_->types["float"] = std::make_shared<ast::SimpleType>(
        lexer::Token(lexer::TokenType::IDENTIFIER, "float", "", 0, 0));
    impl_->types["float32"] = impl_->types["float"];
    impl_->types["float64"] = std::make_shared<ast::SimpleType>(
        lexer::Token(lexer::TokenType::IDENTIFIER, "float64", "", 0, 0));
    impl_->types["bool"] = std::make_shared<ast::SimpleType>(
        lexer::Token(lexer::TokenType::IDENTIFIER, "bool", "", 0, 0));
    impl_->types["string"] = std::make_shared<ast::SimpleType>(
        lexer::Token(lexer::TokenType::IDENTIFIER, "string", "", 0, 0));
    impl_->types["void"] = std::make_shared<ast::SimpleType>(
        lexer::Token(lexer::TokenType::IDENTIFIER, "void", "", 0, 0));
    impl_->types["null"] = std::make_shared<ast::SimpleType>(
        lexer::Token(lexer::TokenType::IDENTIFIER, "null", "", 0, 0));
}

TypeRegistry::~TypeRegistry() = default;

Result<bool, CompilerError> TypeRegistry::registerType(
    const std::string& name, ast::TypePtr type) {
    
    if (impl_->types.count(name)) {
        return Result<bool, CompilerError>::Err(
            CompilerError("Type already registered: " + name));
    }
    
    impl_->types[name] = type;
    return Result<bool, CompilerError>::Ok(true);
}

Result<bool, CompilerError> TypeRegistry::registerAlias(
    const std::string& alias, ast::TypePtr type) {
    
    impl_->aliases[alias] = type;
    return Result<bool, CompilerError>::Ok(true);
}

Option<ast::TypePtr> TypeRegistry::lookupType(const std::string& name) const {
    auto it = impl_->types.find(name);
    if (it != impl_->types.end()) {
        return Option<ast::TypePtr>::Some(it->second);
    }
    return Option<ast::TypePtr>::None();
}

Option<ast::TypePtr> TypeRegistry::resolveAlias(const std::string& alias) const {
    auto it = impl_->aliases.find(alias);
    if (it != impl_->aliases.end()) {
        return Option<ast::TypePtr>::Some(it->second);
    }
    return Option<ast::TypePtr>::None();
}

Result<bool, CompilerError> TypeRegistry::registerGenericType(
    const std::string& name,
    const std::vector<TypeParameter>& params,
    ast::TypePtr definition) {
    
    impl_->genericTypes[name] = params;
    impl_->types[name] = definition;
    return Result<bool, CompilerError>::Ok(true);
}

Option<std::vector<TypeParameter>> TypeRegistry::getTypeParameters(
    const std::string& name) const {
    
    auto it = impl_->genericTypes.find(name);
    if (it != impl_->genericTypes.end()) {
        return Option<std::vector<TypeParameter>>::Some(it->second);
    }
    return Option<std::vector<TypeParameter>>::None();
}

ast::TypePtr TypeRegistry::getInt32Type() const {
    return impl_->types.at("int");
}

ast::TypePtr TypeRegistry::getInt64Type() const {
    return impl_->types.at("int64");
}

ast::TypePtr TypeRegistry::getFloat32Type() const {
    return impl_->types.at("float");
}

ast::TypePtr TypeRegistry::getFloat64Type() const {
    return impl_->types.at("float64");
}

ast::TypePtr TypeRegistry::getBoolType() const {
    return impl_->types.at("bool");
}

ast::TypePtr TypeRegistry::getStringType() const {
    return impl_->types.at("string");
}

ast::TypePtr TypeRegistry::getVoidType() const {
    return impl_->types.at("void");
}

ast::TypePtr TypeRegistry::getNullType() const {
    return impl_->types.at("null");
}

ast::TypePtr TypeRegistry::makeArrayType(ast::TypePtr elementType) {
    return std::make_shared<ast::GenericType>(
        lexer::Token(lexer::TokenType::IDENTIFIER, "Array", "", 0, 0),
        "Array",
        std::vector<ast::TypePtr>{elementType}
    );
}

ast::TypePtr TypeRegistry::makePointerType(ast::TypePtr pointeeType) {
    return std::make_shared<ast::PointerType>(pointeeType);
}

ast::TypePtr TypeRegistry::makeReferenceType(ast::TypePtr referentType) {
    return std::make_shared<ast::ReferenceType>(referentType);
}

ast::TypePtr TypeRegistry::makeOptionType(ast::TypePtr innerType) {
    return std::make_shared<ast::GenericType>(
        lexer::Token(lexer::TokenType::IDENTIFIER, "Option", "", 0, 0),
        "Option",
        std::vector<ast::TypePtr>{innerType}
    );
}

ast::TypePtr TypeRegistry::makeResultType(ast::TypePtr okType, ast::TypePtr errType) {
    return std::make_shared<ast::GenericType>(
        lexer::Token(lexer::TokenType::IDENTIFIER, "Result", "", 0, 0),
        "Result",
        std::vector<ast::TypePtr>{okType, errType}
    );
}

// ============================================================================
// TypePrinter Implementation
// ============================================================================

std::string TypePrinter::toString(ast::TypePtr type) {
    if (!type) return "<null>";
    
    if (auto simpleType = std::dynamic_pointer_cast<ast::SimpleType>(type)) {
        return simpleType->name;
    }
    
    if (auto genericType = std::dynamic_pointer_cast<ast::GenericType>(type)) {
        std::ostringstream oss;
        oss << genericType->name << "<";
        for (size_t i = 0; i < genericType->typeArgs.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << toString(genericType->typeArgs[i]);
        }
        oss << ">";
        return oss.str();
    }
    
    return "<unknown>";
}

std::string TypePrinter::toDebugString(ast::TypePtr type) {
    return toString(type);
}

std::string TypePrinter::toMangledName(ast::TypePtr type) {
    // TODO: Implement proper name mangling
    return toString(type);
}

// ============================================================================
// TypeUtils Implementation
// ============================================================================

bool TypeUtils::isIntegral(ast::TypePtr type) {
    if (auto simple = std::dynamic_pointer_cast<ast::SimpleType>(type)) {
        return simple->name == "int" || simple->name == "int32" || 
               simple->name == "int64" || simple->name == "uint32" ||
               simple->name == "uint64";
    }
    return false;
}

bool TypeUtils::isFloating(ast::TypePtr type) {
    if (auto simple = std::dynamic_pointer_cast<ast::SimpleType>(type)) {
        return simple->name == "float" || simple->name == "float32" ||
               simple->name == "float64" || simple->name == "double";
    }
    return false;
}

bool TypeUtils::isNumeric(ast::TypePtr type) {
    return isIntegral(type) || isFloating(type);
}

bool TypeUtils::isSigned(ast::TypePtr type) {
    if (auto simple = std::dynamic_pointer_cast<ast::SimpleType>(type)) {
        return simple->name != "uint32" && simple->name != "uint64";
    }
    return false;
}

bool TypeUtils::isPointer(ast::TypePtr type) {
    return std::dynamic_pointer_cast<ast::PointerType>(type) != nullptr;
}

bool TypeUtils::isReference(ast::TypePtr type) {
    return std::dynamic_pointer_cast<ast::ReferenceType>(type) != nullptr;
}

bool TypeUtils::isArray(ast::TypePtr type) {
    if (auto gen = std::dynamic_pointer_cast<ast::GenericType>(type)) {
        return gen->name == "Array" || gen->name == "Vec";
    }
    return false;
}

bool TypeUtils::isFunction(ast::TypePtr type) {
    return std::dynamic_pointer_cast<ast::FunctionType>(type) != nullptr;
}

bool TypeUtils::isGeneric(ast::TypePtr type) {
    return std::dynamic_pointer_cast<ast::GenericType>(type) != nullptr;
}

bool TypeUtils::isVoid(ast::TypePtr type) {
    if (auto simple = std::dynamic_pointer_cast<ast::SimpleType>(type)) {
        return simple->name == "void";
    }
    return false;
}

} // namespace type
} // namespace tocin
