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
    if (auto simpleType = std::dynamic_pointer_cast<ast::BasicType>(type)) {
        auto resolved = impl_->registry.lookupType(simpleType->toString());
        if (resolved.isNone()) {
            return Result<ast::TypePtr, CompilerError>::Err(
                CompilerError("Unknown type: " + simpleType->toString()));
        }
        return Result<ast::TypePtr, CompilerError>::Ok(type);
    }
    
    if (auto genericType = std::dynamic_pointer_cast<ast::GenericType>(type)) {
        // Validate all type arguments
        for (const auto& arg : genericType->typeArguments) {
            auto result = validateType(arg);
            if (result.isErr()) {
                return result;
            }
        }
        
        // Check generic instantiation validity
        auto validation = validateGenericInstantiation(type, genericType->typeArguments);
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
    if (auto nullType = std::dynamic_pointer_cast<ast::BasicType>(sub)) {
        if (nullType->toString() == "null" && std::dynamic_pointer_cast<ast::PointerType>(super)) {
            return Result<bool, CompilerError>::Ok(true);
        }
    }
    
    // Reflexivity: T <: T
    if (typesEqual(sub, super)) {
        return Result<bool, CompilerError>::Ok(true);
    }
    
    // Class inheritance: check if sub is a subclass of super
    if (auto subClass = std::dynamic_pointer_cast<ast::ClassType>(sub)) {
        if (auto superClass = std::dynamic_pointer_cast<ast::ClassType>(super)) {
            // Check direct inheritance chain
            auto current = impl_->registry.getClassInfo(subClass->name);
            while (current.isSome()) {
                if (current.unwrap().superclass == superClass->name) {
                    return Result<bool, CompilerError>::Ok(true);
                }
                // Move up the inheritance chain
                if (current.unwrap().superclass.empty()) {
                    break;
                }
                current = impl_->registry.getClassInfo(current.unwrap().superclass);
            }
        }
    }
    
    // Trait implementation: check if sub implements super trait
    if (auto traitType = std::dynamic_pointer_cast<ast::TraitType>(super)) {
        auto impls = impl_->registry.getTraitImpls(sub);
        if (impls.isSome()) {
            for (const auto& impl : impls.unwrap()) {
                if (impl.traitName == traitType->name) {
                    return Result<bool, CompilerError>::Ok(true);
                }
            }
        }
    }
    
    // Generic type variance
    if (auto subGen = std::dynamic_pointer_cast<ast::GenericType>(sub)) {
        if (auto superGen = std::dynamic_pointer_cast<ast::GenericType>(super)) {
            if (subGen->name == superGen->name && 
                subGen->typeArguments.size() == superGen->typeArguments.size()) {
                // For now, assume invariance - all type args must be equal
                // TODO: Support covariance/contravariance annotations
                for (size_t i = 0; i < subGen->typeArguments.size(); ++i) {
                    if (!typesEqual(subGen->typeArguments[i], superGen->typeArguments[i])) {
                        return Result<bool, CompilerError>::Ok(false);
                    }
                }
                return Result<bool, CompilerError>::Ok(true);
            }
        }
    }
    
    // Function subtyping (contravariant in arguments, covariant in return type)
    if (auto subFn = std::dynamic_pointer_cast<ast::FunctionType>(sub)) {
        if (auto superFn = std::dynamic_pointer_cast<ast::FunctionType>(super)) {
            if (subFn->parameterTypes.size() != superFn->parameterTypes.size()) {
                return Result<bool, CompilerError>::Ok(false);
            }
            
            // Parameters are contravariant
            for (size_t i = 0; i < subFn->parameterTypes.size(); ++i) {
                auto paramCheck = isSubtype(superFn->parameterTypes[i], subFn->parameterTypes[i]);
                if (paramCheck.isErr() || !paramCheck.unwrap()) {
                    return Result<bool, CompilerError>::Ok(false);
                }
            }
            
            // Return type is covariant
            return isSubtype(subFn->returnType, superFn->returnType);
        }
    }
    
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
    if (!expr) {
        return Result<ast::TypePtr, CompilerError>::Err(
            CompilerError("Cannot infer type of null expression"));
    }
    
    // Literal expressions
    if (auto lit = std::dynamic_pointer_cast<ast::LiteralExpr>(expr)) {
        if (lit->value.type == lexer::TokenType::NUMBER) {
            // Check if it's a float or int
            if (lit->value.value.find('.') != std::string::npos) {
                return Result<ast::TypePtr, CompilerError>::Ok(impl_->registry.getFloatType());
            }
            return Result<ast::TypePtr, CompilerError>::Ok(impl_->registry.getIntType());
        } else if (lit->value.type == lexer::TokenType::STRING) {
            return Result<ast::TypePtr, CompilerError>::Ok(impl_->registry.getStringType());
        } else if (lit->value.type == lexer::TokenType::TRUE || 
                   lit->value.type == lexer::TokenType::FALSE) {
            return Result<ast::TypePtr, CompilerError>::Ok(impl_->registry.getBoolType());
        }
    }
    
    // Binary expressions
    if (auto bin = std::dynamic_pointer_cast<ast::BinaryExpr>(expr)) {
        auto leftType = inferType(bin->left);
        auto rightType = inferType(bin->right);
        
        if (leftType.isErr() || rightType.isErr()) {
            return Result<ast::TypePtr, CompilerError>::Err(
                CompilerError("Cannot infer types of operands"));
        }
        
        // Arithmetic operators return numeric types
        if (bin->op.type == lexer::TokenType::PLUS || 
            bin->op.type == lexer::TokenType::MINUS ||
            bin->op.type == lexer::TokenType::STAR ||
            bin->op.type == lexer::TokenType::SLASH) {
            return unifyTypes(leftType.unwrap(), rightType.unwrap());
        }
        
        // Comparison operators return bool
        if (bin->op.type == lexer::TokenType::EQUAL_EQUAL ||
            bin->op.type == lexer::TokenType::BANG_EQUAL ||
            bin->op.type == lexer::TokenType::LESS ||
            bin->op.type == lexer::TokenType::LESS_EQUAL ||
            bin->op.type == lexer::TokenType::GREATER ||
            bin->op.type == lexer::TokenType::GREATER_EQUAL) {
            return Result<ast::TypePtr, CompilerError>::Ok(impl_->registry.getBoolType());
        }
    }
    
    // Unary expressions
    if (auto un = std::dynamic_pointer_cast<ast::UnaryExpr>(expr)) {
        auto rightType = inferType(un->right);
        if (rightType.isErr()) {
            return rightType;
        }
        
        if (un->op.type == lexer::TokenType::BANG) {
            return Result<ast::TypePtr, CompilerError>::Ok(impl_->registry.getBoolType());
        }
        return rightType;
    }
    
    // Variable expressions - look up in type registry
    if (auto var = std::dynamic_pointer_cast<ast::VariableExpr>(expr)) {
        auto type = impl_->registry.lookupVariable(var->name.value);
        if (type.isSome()) {
            return Result<ast::TypePtr, CompilerError>::Ok(type.unwrap());
        }
        return Result<ast::TypePtr, CompilerError>::Err(
            CompilerError("Unknown variable: " + var->name.value));
    }
    
    // Call expressions
    if (auto call = std::dynamic_pointer_cast<ast::CallExpr>(expr)) {
        auto calleeType = inferType(call->callee);
        if (calleeType.isErr()) {
            return calleeType;
        }
        
        if (auto fnType = std::dynamic_pointer_cast<ast::FunctionType>(calleeType.unwrap())) {
            return Result<ast::TypePtr, CompilerError>::Ok(fnType->returnType);
        }
        return Result<ast::TypePtr, CompilerError>::Err(
            CompilerError("Cannot call non-function type"));
    }
    
    // Lambda expressions
    if (auto lambda = std::dynamic_pointer_cast<ast::LambdaExpr>(expr)) {
        std::vector<ast::TypePtr> paramTypes;
        for (const auto& param : lambda->params) {
            paramTypes.push_back(param.type);
        }
        return Result<ast::TypePtr, CompilerError>::Ok(
            std::make_shared<ast::FunctionType>(paramTypes, lambda->returnType));
    }
    
    // Array/List expressions
    if (auto list = std::dynamic_pointer_cast<ast::ListExpr>(expr)) {
        if (list->elements.empty()) {
            // Empty array, cannot infer element type
            return Result<ast::TypePtr, CompilerError>::Err(
                CompilerError("Cannot infer type of empty array"));
        }
        
        auto elemType = inferType(list->elements[0]);
        if (elemType.isErr()) {
            return elemType;
        }
        
        return Result<ast::TypePtr, CompilerError>::Ok(
            std::make_shared<ast::ArrayType>(elemType.unwrap()));
    }
    
    // Default: return void type
    return Result<ast::TypePtr, CompilerError>::Ok(impl_->registry.getVoidType());
}

Result<ast::TypePtr, CompilerError> EnhancedTypeChecker::unifyTypes(
    ast::TypePtr t1, ast::TypePtr t2) {
    
    if (typesEqual(t1, t2)) {
        return Result<ast::TypePtr, CompilerError>::Ok(t1);
    }
    
    // Unification algorithm using Robinson's algorithm
    
    // If either type is a type variable, bind it
    if (auto tv1 = std::dynamic_pointer_cast<ast::TypeVariable>(t1)) {
        if (!occursIn(tv1->name, t2)) {
            return Result<ast::TypePtr, CompilerError>::Ok(t2);
        }
        return Result<ast::TypePtr, CompilerError>::Err(
            CompilerError("Circular type dependency"));
    }
    
    if (auto tv2 = std::dynamic_pointer_cast<ast::TypeVariable>(t2)) {
        if (!occursIn(tv2->name, t1)) {
            return Result<ast::TypePtr, CompilerError>::Ok(t1);
        }
        return Result<ast::TypePtr, CompilerError>::Err(
            CompilerError("Circular type dependency"));
    }
    
    // Numeric type unification (int can be promoted to float)
    if (isNumericType(t1) && isNumericType(t2)) {
        if (impl_->registry.isFloatType(t1) || impl_->registry.isFloatType(t2)) {
            return Result<ast::TypePtr, CompilerError>::Ok(impl_->registry.getFloatType());
        }
        return Result<ast::TypePtr, CompilerError>::Ok(impl_->registry.getIntType());
    }
    
    // Function type unification
    if (auto fn1 = std::dynamic_pointer_cast<ast::FunctionType>(t1)) {
        if (auto fn2 = std::dynamic_pointer_cast<ast::FunctionType>(t2)) {
            if (fn1->parameterTypes.size() != fn2->parameterTypes.size()) {
                return Result<ast::TypePtr, CompilerError>::Err(
                    CompilerError("Function arity mismatch"));
            }
            
            std::vector<ast::TypePtr> unifiedParams;
            for (size_t i = 0; i < fn1->parameterTypes.size(); ++i) {
                auto unified = unifyTypes(fn1->parameterTypes[i], fn2->parameterTypes[i]);
                if (unified.isErr()) {
                    return unified;
                }
                unifiedParams.push_back(unified.unwrap());
            }
            
            auto unifiedReturn = unifyTypes(fn1->returnType, fn2->returnType);
            if (unifiedReturn.isErr()) {
                return unifiedReturn;
            }
            
            return Result<ast::TypePtr, CompilerError>::Ok(
                std::make_shared<ast::FunctionType>(unifiedParams, unifiedReturn.unwrap()));
        }
    }
    
    // Array type unification
    if (auto arr1 = std::dynamic_pointer_cast<ast::ArrayType>(t1)) {
        if (auto arr2 = std::dynamic_pointer_cast<ast::ArrayType>(t2)) {
            auto unifiedElem = unifyTypes(arr1->elementType, arr2->elementType);
            if (unifiedElem.isErr()) {
                return unifiedElem;
            }
            return Result<ast::TypePtr, CompilerError>::Ok(
                std::make_shared<ast::ArrayType>(unifiedElem.unwrap()));
        }
    }
    
    // Generic type unification
    if (auto gen1 = std::dynamic_pointer_cast<ast::GenericType>(t1)) {
        if (auto gen2 = std::dynamic_pointer_cast<ast::GenericType>(t2)) {
            if (gen1->name != gen2->name || 
                gen1->typeArguments.size() != gen2->typeArguments.size()) {
                return Result<ast::TypePtr, CompilerError>::Err(
                    CompilerError("Generic types don't match"));
            }
            
            std::vector<ast::TypePtr> unifiedArgs;
            for (size_t i = 0; i < gen1->typeArguments.size(); ++i) {
                auto unified = unifyTypes(gen1->typeArguments[i], gen2->typeArguments[i]);
                if (unified.isErr()) {
                    return unified;
                }
                unifiedArgs.push_back(unified.unwrap());
            }
            
            auto result = std::make_shared<ast::GenericType>(gen1->name);
            result->typeArguments = unifiedArgs;
            return Result<ast::TypePtr, CompilerError>::Ok(result);
        }
    }
    
    // Check subtyping
    auto sub1 = isSubtype(t1, t2);
    if (sub1.isOk() && sub1.unwrap()) {
        return Result<ast::TypePtr, CompilerError>::Ok(t2);
    }
    
    auto sub2 = isSubtype(t2, t1);
    if (sub2.isOk() && sub2.unwrap()) {
        return Result<ast::TypePtr, CompilerError>::Ok(t1);
    }
    
    return Result<ast::TypePtr, CompilerError>::Err(
        CompilerError("Cannot unify incompatible types"));
}

Result<bool, CompilerError> EnhancedTypeChecker::checkCircularDependency(ast::TypePtr type) {
    std::unordered_set<std::string> visited;
    return checkCircularDependencyHelper(type, visited);
}

Result<bool, CompilerError> EnhancedTypeChecker::checkCircularDependencyHelper(
    ast::TypePtr type, std::unordered_set<std::string>& visited) {
    
    if (!type) {
        return Result<bool, CompilerError>::Ok(false);
    }
    
    // Check simple types
    if (auto simple = std::dynamic_pointer_cast<ast::BasicType>(type)) {
        if (visited.count(simple->toString())) {
            return Result<bool, CompilerError>::Err(
                CompilerError("Circular type dependency detected: " + simple->toString()));
        }
        visited.insert(simple->toString());
        
        // Check if this type is defined in terms of itself
        auto typeDef = impl_->registry.getTypeDefinition(simple->toString());
        if (typeDef.isSome()) {
            return checkCircularDependencyHelper(typeDef.unwrap(), visited);
        }
        
        visited.erase(simple->toString());
    }
    
    // Check class types
    if (auto classType = std::dynamic_pointer_cast<ast::ClassType>(type)) {
        if (visited.count(classType->name)) {
            return Result<bool, CompilerError>::Err(
                CompilerError("Circular type dependency in class: " + classType->name));
        }
        visited.insert(classType->name);
        
        auto classInfo = impl_->registry.getClassInfo(classType->name);
        if (classInfo.isSome()) {
            // Check fields
            for (const auto& [fieldName, fieldType] : classInfo.unwrap().fields) {
                auto check = checkCircularDependencyHelper(fieldType, visited);
                if (check.isErr()) {
                    return check;
                }
            }
        }
        
        visited.erase(classType->name);
    }
    
    // Check array types
    if (auto arr = std::dynamic_pointer_cast<ast::ArrayType>(type)) {
        return checkCircularDependencyHelper(arr->elementType, visited);
    }
    
    // Check pointer types (pointers break cycles)
    if (auto ptr = std::dynamic_pointer_cast<ast::PointerType>(type)) {
        // Pointers don't need to be checked as they break circular references
        return Result<bool, CompilerError>::Ok(false);
    }
    
    // Check generic types
    if (auto gen = std::dynamic_pointer_cast<ast::GenericType>(type)) {
        for (const auto& arg : gen->typeArguments) {
            auto check = checkCircularDependencyHelper(arg, visited);
            if (check.isErr()) {
                return check;
            }
        }
    }
    
    return Result<bool, CompilerError>::Ok(false);
}

Option<size_t> EnhancedTypeChecker::getTypeSize(ast::TypePtr type) const {
    if (!type) {
        return Option<size_t>::None();
    }
    
    // Primitive types
    if (auto simple = std::dynamic_pointer_cast<ast::BasicType>(type)) {
        if (simple->toString() == "bool" || simple->toString() == "i8" || simple->toString() == "u8") {
            return Option<size_t>::Some(1);
        }
        if (simple->toString() == "i16" || simple->toString() == "u16") {
            return Option<size_t>::Some(2);
        }
        if (simple->toString() == "i32" || simple->toString() == "u32" || simple->toString() == "f32" || 
            simple->toString() == "int" || simple->toString() == "float") {
            return Option<size_t>::Some(4);
        }
        if (simple->toString() == "i64" || simple->toString() == "u64" || simple->toString() == "f64" || 
            simple->toString() == "double") {
            return Option<size_t>::Some(8);
        }
        if (simple->toString() == "void") {
            return Option<size_t>::Some(0);
        }
    }
    
    // Pointer types (architecture dependent, assuming 64-bit)
    if (std::dynamic_pointer_cast<ast::PointerType>(type)) {
        return Option<size_t>::Some(8);  // 64-bit pointers
    }
    
    // Reference types (same as pointers)
    if (std::dynamic_pointer_cast<ast::ReferenceType>(type)) {
        return Option<size_t>::Some(8);
    }
    
    // Array types
    if (auto arr = std::dynamic_pointer_cast<ast::ArrayType>(type)) {
        auto elemSize = getTypeSize(arr->elementType);
        if (elemSize.isSome() && arr->size > 0) {
            return Option<size_t>::Some(elemSize.unwrap() * arr->size);
        }
        // Dynamic arrays are pointer-sized
        return Option<size_t>::Some(8);
    }
    
    // Class/struct types - sum of field sizes + padding
    if (auto classType = std::dynamic_pointer_cast<ast::ClassType>(type)) {
        auto classInfo = impl_->registry.getClassInfo(classType->name);
        if (classInfo.isSome()) {
            size_t totalSize = 0;
            size_t maxAlign = 1;
            
            for (const auto& [fieldName, fieldType] : classInfo.unwrap().fields) {
                auto fieldSize = getTypeSize(fieldType);
                auto fieldAlign = getTypeAlignment(fieldType);
                
                if (fieldSize.isSome() && fieldAlign.isSome()) {
                    // Add padding for alignment
                    size_t align = fieldAlign.unwrap();
                    maxAlign = std::max(maxAlign, align);
                    
                    if (totalSize % align != 0) {
                        totalSize += align - (totalSize % align);
                    }
                    
                    totalSize += fieldSize.unwrap();
                }
            }
            
            // Add final padding to align struct size
            if (totalSize % maxAlign != 0) {
                totalSize += maxAlign - (totalSize % maxAlign);
            }
            
            return Option<size_t>::Some(totalSize);
        }
    }
    
    // Function types are pointer-sized
    if (std::dynamic_pointer_cast<ast::FunctionType>(type)) {
        return Option<size_t>::Some(8);
    }
    
    return Option<size_t>::None();
}

Option<size_t> EnhancedTypeChecker::getTypeAlignment(ast::TypePtr type) const {
    if (!type) {
        return Option<size_t>::None();
    }
    
    // For most types, alignment equals size (up to 8 bytes)
    auto size = getTypeSize(type);
    if (size.isSome()) {
        size_t s = size.unwrap();
        if (s == 0) return Option<size_t>::Some(1);
        if (s <= 8) return Option<size_t>::Some(s);
        return Option<size_t>::Some(8);  // Max alignment on most platforms
    }
    
    // Class types - alignment is max of field alignments
    if (auto classType = std::dynamic_pointer_cast<ast::ClassType>(type)) {
        auto classInfo = impl_->registry.getClassInfo(classType->name);
        if (classInfo.isSome()) {
            size_t maxAlign = 1;
            for (const auto& [fieldName, fieldType] : classInfo.unwrap().fields) {
                auto fieldAlign = getTypeAlignment(fieldType);
                if (fieldAlign.isSome()) {
                    maxAlign = std::max(maxAlign, fieldAlign.unwrap());
                }
            }
            return Option<size_t>::Some(maxAlign);
        }
    }
    
    return Option<size_t>::Some(1);  // Default alignment
}

bool EnhancedTypeChecker::isNullable(ast::TypePtr type) const {
    // Pointer types and Option types are nullable
    return std::dynamic_pointer_cast<ast::PointerType>(type) != nullptr;
}

bool EnhancedTypeChecker::isCopyable(ast::TypePtr type) const {
    if (!type) return false;
    
    // Check for move-only types
    if (auto classType = std::dynamic_pointer_cast<ast::ClassType>(type)) {
        auto classInfo = impl_->registry.getClassInfo(classType->name);
        if (classInfo.isSome()) {
            // Check if class has a copy constructor or is marked as move-only
            const auto& info = classInfo.unwrap();
            if (info.isMoveOnly) {
                return false;
            }
        }
    }
    
    // Unique pointers are move-only
    if (auto ptrType = std::dynamic_pointer_cast<ast::PointerType>(type)) {
        if (ptrType->isUnique) {
            return false;
        }
    }
    
    // File handles and other resource types are typically move-only
    if (auto simple = std::dynamic_pointer_cast<ast::BasicType>(type)) {
        if (simple->toString() == "File" || simple->toString() == "Socket" || 
            simple->toString() == "Mutex" || simple->toString() == "Thread") {
            return false;
        }
    }
    
    // Default: types are copyable
    return true;
}

bool EnhancedTypeChecker::isMovable(ast::TypePtr type) const {
    // All types are movable by default
    return true;
}

bool EnhancedTypeChecker::typesEqual(ast::TypePtr t1, ast::TypePtr t2) const {
    if (!t1 && !t2) return true;
    if (!t1 || !t2) return false;
    
    // Check if same pointer
    if (t1 == t2) return true;
    
    // SimpleType equality
    if (auto s1 = std::dynamic_pointer_cast<ast::BasicType>(t1)) {
        if (auto s2 = std::dynamic_pointer_cast<ast::BasicType>(t2)) {
            return s1->name == s2->name;
        }
    }
    
    // PointerType equality
    if (auto p1 = std::dynamic_pointer_cast<ast::PointerType>(t1)) {
        if (auto p2 = std::dynamic_pointer_cast<ast::PointerType>(t2)) {
            return p1->isUnique == p2->isUnique && typesEqual(p1->pointeeType, p2->pointeeType);
        }
    }
    
    // ReferenceType equality
    if (auto r1 = std::dynamic_pointer_cast<ast::ReferenceType>(t1)) {
        if (auto r2 = std::dynamic_pointer_cast<ast::ReferenceType>(t2)) {
            return r1->isMutable == r2->isMutable && typesEqual(r1->referencedType, r2->referencedType);
        }
    }
    
    // ArrayType equality
    if (auto a1 = std::dynamic_pointer_cast<ast::ArrayType>(t1)) {
        if (auto a2 = std::dynamic_pointer_cast<ast::ArrayType>(t2)) {
            return a1->size == a2->size && typesEqual(a1->elementType, a2->elementType);
        }
    }
    
    // FunctionType equality
    if (auto f1 = std::dynamic_pointer_cast<ast::FunctionType>(t1)) {
        if (auto f2 = std::dynamic_pointer_cast<ast::FunctionType>(t2)) {
            if (f1->parameterTypes.size() != f2->parameterTypes.size()) return false;
            if (!typesEqual(f1->returnType, f2->returnType)) return false;
            
            for (size_t i = 0; i < f1->parameterTypes.size(); ++i) {
                if (!typesEqual(f1->parameterTypes[i], f2->parameterTypes[i])) return false;
            }
            return true;
        }
    }
    
    // GenericType equality
    if (auto g1 = std::dynamic_pointer_cast<ast::GenericType>(t1)) {
        if (auto g2 = std::dynamic_pointer_cast<ast::GenericType>(t2)) {
            if (g1->name != g2->name || g1->typeArguments.size() != g2->typeArguments.size()) {
                return false;
            }
            for (size_t i = 0; i < g1->typeArguments.size(); ++i) {
                if (!typesEqual(g1->typeArguments[i], g2->typeArguments[i])) return false;
            }
            return true;
        }
    }
    
    // ClassType equality
    if (auto c1 = std::dynamic_pointer_cast<ast::ClassType>(t1)) {
        if (auto c2 = std::dynamic_pointer_cast<ast::ClassType>(t2)) {
            return c1->name == c2->name;
        }
    }
    
    // TraitType equality
    if (auto tr1 = std::dynamic_pointer_cast<ast::TraitType>(t1)) {
        if (auto tr2 = std::dynamic_pointer_cast<ast::TraitType>(t2)) {
            return tr1->name == tr2->name;
        }
    }
    
    return false;
}

ast::TypePtr EnhancedTypeChecker::substituteTypeParameters(
    ast::TypePtr type,
    const std::unordered_map<std::string, ast::TypePtr>& substitutions) {
    
    if (!type) return type;
    
    // TypeVariable substitution
    if (auto tv = std::dynamic_pointer_cast<ast::TypeVariable>(type)) {
        auto it = substitutions.find(tv->name);
        if (it != substitutions.end()) {
            return it->second;
        }
        return type;
    }
    
    // SimpleType might be a type parameter
    if (auto simple = std::dynamic_pointer_cast<ast::BasicType>(type)) {
        auto it = substitutions.find(simple->toString());
        if (it != substitutions.end()) {
            return it->second;
        }
        return type;
    }
    
    // PointerType substitution
    if (auto ptr = std::dynamic_pointer_cast<ast::PointerType>(type)) {
        auto substituted = substituteTypeParameters(ptr->pointeeType, substitutions);
        if (substituted != ptr->pointeeType) {
            auto newPtr = std::make_shared<ast::PointerType>(substituted);
            newPtr->isUnique = ptr->isUnique;
            return newPtr;
        }
        return type;
    }
    
    // ReferenceType substitution
    if (auto ref = std::dynamic_pointer_cast<ast::ReferenceType>(type)) {
        auto substituted = substituteTypeParameters(ref->referencedType, substitutions);
        if (substituted != ref->referencedType) {
            auto newRef = std::make_shared<ast::ReferenceType>(substituted);
            newRef->isMutable = ref->isMutable;
            return newRef;
        }
        return type;
    }
    
    // ArrayType substitution
    if (auto arr = std::dynamic_pointer_cast<ast::ArrayType>(type)) {
        auto substituted = substituteTypeParameters(arr->elementType, substitutions);
        if (substituted != arr->elementType) {
            auto newArr = std::make_shared<ast::ArrayType>(substituted);
            newArr->size = arr->size;
            return newArr;
        }
        return type;
    }
    
    // FunctionType substitution
    if (auto fn = std::dynamic_pointer_cast<ast::FunctionType>(type)) {
        std::vector<ast::TypePtr> newParams;
        bool changed = false;
        
        for (const auto& param : fn->parameterTypes) {
            auto substituted = substituteTypeParameters(param, substitutions);
            newParams.push_back(substituted);
            if (substituted != param) changed = true;
        }
        
        auto newReturn = substituteTypeParameters(fn->returnType, substitutions);
        if (newReturn != fn->returnType) changed = true;
        
        if (changed) {
            return std::make_shared<ast::FunctionType>(newParams, newReturn);
        }
        return type;
    }
    
    // GenericType substitution
    if (auto gen = std::dynamic_pointer_cast<ast::GenericType>(type)) {
        std::vector<ast::TypePtr> newArgs;
        bool changed = false;
        
        for (const auto& arg : gen->typeArguments) {
            auto substituted = substituteTypeParameters(arg, substitutions);
            newArgs.push_back(substituted);
            if (substituted != arg) changed = true;
        }
        
        if (changed) {
            auto newGen = std::make_shared<ast::GenericType>(gen->name);
            newGen->typeArguments = newArgs;
            return newGen;
        }
        return type;
    }
    
    // For other types, return as-is
    return type;
}

Result<bool, CompilerError> EnhancedTypeChecker::validateMethodSignature(
    const std::string& methodName,
    ast::TypePtr signature,
    const Trait& trait) {
    
    if (!signature) {
        return Result<bool, CompilerError>::Err(
            CompilerError("Null signature for method: " + methodName));
    }
    
    // Check that the signature is a function type
    auto fnType = std::dynamic_pointer_cast<ast::FunctionType>(signature);
    if (!fnType) {
        return Result<bool, CompilerError>::Err(
            CompilerError("Method signature must be a function type"));
    }
    
    // Verify the method exists in the trait
    auto it = trait.methods.find(methodName);
    if (it == trait.methods.end()) {
        return Result<bool, CompilerError>::Err(
            CompilerError("Method not declared in trait: " + methodName));
    }
    
    auto expectedSig = std::dynamic_pointer_cast<ast::FunctionType>(it->second);
    if (!expectedSig) {
        return Result<bool, CompilerError>::Err(
            CompilerError("Trait method signature is not a function type"));
    }
    
    // Check parameter count
    if (fnType->parameterTypes.size() != expectedSig->parameterTypes.size()) {
        return Result<bool, CompilerError>::Err(
            CompilerError("Method parameter count mismatch for: " + methodName));
    }
    
    // Check parameter types
    for (size_t i = 0; i < fnType->parameterTypes.size(); ++i) {
        if (!typesEqual(fnType->parameterTypes[i], expectedSig->parameterTypes[i])) {
            return Result<bool, CompilerError>::Err(
                CompilerError("Method parameter type mismatch at position " + 
                            std::to_string(i) + " for: " + methodName));
        }
    }
    
    // Check return type
    if (!typesEqual(fnType->returnType, expectedSig->returnType)) {
        return Result<bool, CompilerError>::Err(
            CompilerError("Method return type mismatch for: " + methodName));
    }
    
    return Result<bool, CompilerError>::Ok(true);
}

// ============================================================================
// Private Helper Methods
// ============================================================================

bool EnhancedTypeChecker::occursIn(const std::string& varName, ast::TypePtr type) const {
    if (!type) return false;
    
    // Check if the type variable occurs in this type
    if (auto tv = std::dynamic_pointer_cast<ast::TypeVariable>(type)) {
        return tv->name == varName;
    }
    
    if (auto simple = std::dynamic_pointer_cast<ast::BasicType>(type)) {
        return simple->toString() == varName;
    }
    
    if (auto ptr = std::dynamic_pointer_cast<ast::PointerType>(type)) {
        return occursIn(varName, ptr->pointeeType);
    }
    
    if (auto ref = std::dynamic_pointer_cast<ast::ReferenceType>(type)) {
        return occursIn(varName, ref->referencedType);
    }
    
    if (auto arr = std::dynamic_pointer_cast<ast::ArrayType>(type)) {
        return occursIn(varName, arr->elementType);
    }
    
    if (auto fn = std::dynamic_pointer_cast<ast::FunctionType>(type)) {
        if (occursIn(varName, fn->returnType)) return true;
        for (const auto& param : fn->parameterTypes) {
            if (occursIn(varName, param)) return true;
        }
        return false;
    }
    
    if (auto gen = std::dynamic_pointer_cast<ast::GenericType>(type)) {
        if (gen->name == varName) return true;
        for (const auto& arg : gen->typeArguments) {
            if (occursIn(varName, arg)) return true;
        }
        return false;
    }
    
    return false;
}

bool EnhancedTypeChecker::isNumericType(ast::TypePtr type) const {
    if (!type) return false;
    
    if (auto simple = std::dynamic_pointer_cast<ast::BasicType>(type)) {
        return simple->toString() == "int" || simple->toString() == "int32" || 
               simple->toString() == "int64" || simple->toString() == "uint32" || 
               simple->toString() == "uint64" || simple->toString() == "i8" || 
               simple->toString() == "i16" || simple->toString() == "i32" || 
               simple->toString() == "i64" || simple->toString() == "u8" || 
               simple->toString() == "u16" || simple->toString() == "u32" || 
               simple->toString() == "u64" || simple->toString() == "float" || 
               simple->toString() == "float32" || simple->toString() == "float64" || 
               simple->toString() == "double" || simple->toString() == "f32" || 
               simple->toString() == "f64";
    }
    
    return false;
}

Result<bool, CompilerError> EnhancedTypeChecker::checkCircularDependencyHelper(
    ast::TypePtr type, std::unordered_set<std::string>& visited) {
    
    if (!type) {
        return Result<bool, CompilerError>::Ok(false);
    }
    
    // Check simple types
    if (auto simple = std::dynamic_pointer_cast<ast::BasicType>(type)) {
        if (visited.count(simple->toString())) {
            return Result<bool, CompilerError>::Err(
                CompilerError("Circular type dependency detected: " + simple->toString()));
        }
        visited.insert(simple->toString());
        
        // Check if this type is defined in terms of itself
        auto typeDef = impl_->registry.getTypeDefinition(simple->toString());
        if (typeDef.isSome()) {
            auto result = checkCircularDependencyHelper(typeDef.unwrap(), visited);
            visited.erase(simple->toString());
            return result;
        }
        
        visited.erase(simple->toString());
    }
    
    // Check class types
    if (auto classType = std::dynamic_pointer_cast<ast::ClassType>(type)) {
        if (visited.count(classType->name)) {
            return Result<bool, CompilerError>::Err(
                CompilerError("Circular type dependency in class: " + classType->name));
        }
        visited.insert(classType->name);
        
        auto classInfo = impl_->registry.getClassInfo(classType->name);
        if (classInfo.isSome()) {
            // Check fields
            for (const auto& [fieldName, fieldType] : classInfo.unwrap().fields) {
                auto check = checkCircularDependencyHelper(fieldType, visited);
                if (check.isErr()) {
                    visited.erase(classType->name);
                    return check;
                }
            }
        }
        
        visited.erase(classType->name);
    }
    
    // Check array types
    if (auto arr = std::dynamic_pointer_cast<ast::ArrayType>(type)) {
        return checkCircularDependencyHelper(arr->elementType, visited);
    }
    
    // Check pointer types (pointers break cycles)
    if (auto ptr = std::dynamic_pointer_cast<ast::PointerType>(type)) {
        // Pointers don't need to be checked as they break circular references
        return Result<bool, CompilerError>::Ok(false);
    }
    
    // Check generic types
    if (auto gen = std::dynamic_pointer_cast<ast::GenericType>(type)) {
        for (const auto& arg : gen->typeArguments) {
            auto check = checkCircularDependencyHelper(arg, visited);
            if (check.isErr()) {
                return check;
            }
        }
    }
    
    return Result<bool, CompilerError>::Ok(false);
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
    // Register built-in types using TypeKind enum
    impl_->types["int"] = std::make_shared<ast::BasicType>(ast::TypeKind::INT);
    impl_->types["int32"] = impl_->types["int"];
    impl_->types["int64"] = std::make_shared<ast::BasicType>(ast::TypeKind::INT);
    impl_->types["float"] = std::make_shared<ast::BasicType>(ast::TypeKind::FLOAT);
    impl_->types["float32"] = impl_->types["float"];
    impl_->types["float64"] = std::make_shared<ast::BasicType>(ast::TypeKind::FLOAT);
    impl_->types["bool"] = std::make_shared<ast::BasicType>(ast::TypeKind::BOOL);
    impl_->types["string"] = std::make_shared<ast::BasicType>(ast::TypeKind::STRING);
    impl_->types["void"] = std::make_shared<ast::BasicType>(ast::TypeKind::VOID);
    impl_->types["null"] = std::make_shared<ast::BasicType>(ast::TypeKind::UNKNOWN); // null as unknown type
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
    
    if (auto simpleType = std::dynamic_pointer_cast<ast::BasicType>(type)) {
        return simpleType->toString();
    }
    
    if (auto genericType = std::dynamic_pointer_cast<ast::GenericType>(type)) {
        std::ostringstream oss;
        oss << genericType->name << "<";
        for (size_t i = 0; i < genericType->typeArguments.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << toString(genericType->typeArguments[i]);
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
    if (!type) return "_Z0v";  // void
    
    // Itanium C++ ABI name mangling (simplified version)
    std::string result;
    
    if (auto simple = std::dynamic_pointer_cast<ast::BasicType>(type)) {
        // Primitive types
        if (simple->toString() == "void") return "v";
        if (simple->toString() == "bool") return "b";
        if (simple->toString() == "char") return "c";
        if (simple->toString() == "int" || simple->toString() == "int32") return "i";
        if (simple->toString() == "int64") return "l";
        if (simple->toString() == "uint32") return "j";
        if (simple->toString() == "uint64") return "m";
        if (simple->toString() == "float" || simple->toString() == "float32") return "f";
        if (simple->toString() == "float64" || simple->toString() == "double") return "d";
        if (simple->toString() == "string") return "Ss";  // std::string
        
        // User-defined types: length + name
        return std::to_string(simple->toString().length()) + simple->toString();
    }
    
    if (auto ptr = std::dynamic_pointer_cast<ast::PointerType>(type)) {
        return "P" + toMangledName(ptr->pointeeType);
    }
    
    if (auto ref = std::dynamic_pointer_cast<ast::ReferenceType>(type)) {
        return "R" + toMangledName(ref->referencedType);
    }
    
    if (auto arr = std::dynamic_pointer_cast<ast::ArrayType>(type)) {
        if (arr->size > 0) {
            return "A" + std::to_string(arr->size) + "_" + toMangledName(arr->elementType);
        }
        return "PA" + toMangledName(arr->elementType);  // Pointer to array
    }
    
    if (auto fn = std::dynamic_pointer_cast<ast::FunctionType>(type)) {
        std::string result = "F";
        result += toMangledName(fn->returnType);
        for (const auto& param : fn->parameterTypes) {
            result += toMangledName(param);
        }
        result += "E";
        return result;
    }
    
    if (auto gen = std::dynamic_pointer_cast<ast::GenericType>(type)) {
        std::string result = std::to_string(gen->name.length()) + gen->name;
        if (!gen->typeArguments.empty()) {
            result += "I";
            for (const auto& arg : gen->typeArguments) {
                result += toMangledName(arg);
            }
            result += "E";
        }
        return result;
    }
    
    if (auto classType = std::dynamic_pointer_cast<ast::ClassType>(type)) {
        return std::to_string(classType->name.length()) + classType->name;
    }
    
    if (auto traitType = std::dynamic_pointer_cast<ast::TraitType>(type)) {
        return std::to_string(traitType->name.length()) + traitType->name;
    }
    
    // Fallback: use toString
    std::string str = toString(type);
    return std::to_string(str.length()) + str;
}

// ============================================================================
// TypeUtils Implementation
// ============================================================================

bool TypeUtils::isIntegral(ast::TypePtr type) {
    if (auto simple = std::dynamic_pointer_cast<ast::BasicType>(type)) {
        return simple->toString() == "int" || simple->toString() == "int32" || 
               simple->toString() == "int64" || simple->toString() == "uint32" ||
               simple->toString() == "uint64";
    }
    return false;
}

bool TypeUtils::isFloating(ast::TypePtr type) {
    if (auto simple = std::dynamic_pointer_cast<ast::BasicType>(type)) {
        return simple->toString() == "float" || simple->toString() == "float32" ||
               simple->toString() == "float64" || simple->toString() == "double";
    }
    return false;
}

bool TypeUtils::isNumeric(ast::TypePtr type) {
    return isIntegral(type) || isFloating(type);
}

bool TypeUtils::isSigned(ast::TypePtr type) {
    if (auto simple = std::dynamic_pointer_cast<ast::BasicType>(type)) {
        return simple->toString() != "uint32" && simple->toString() != "uint64";
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
    if (auto simple = std::dynamic_pointer_cast<ast::BasicType>(type)) {
        return simple->toString() == "void";
    }
    return false;
}

} // namespace type
} // namespace tocin
