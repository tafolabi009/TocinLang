#include "traits.h"
#include <iostream>
#include <sstream>

namespace type_checker {

TraitChecker::TraitChecker(error::ErrorHandler& errorHandler)
    : errorHandler_(errorHandler) {
}

TraitChecker::~TraitChecker() {
}

bool TraitChecker::registerTrait(ast::TraitDeclPtr traitDecl) {
    if (!traitDecl) return false;
    
    std::string traitName = traitDecl->getName();
    if (isTraitRegistered(traitName)) {
        reportTraitError("Trait '" + traitName + "' is already registered");
        return false;
    }
    
    traits_[traitName] = traitDecl;
    return true;
}

bool TraitChecker::isTraitRegistered(const std::string& traitName) const {
    return traits_.find(traitName) != traits_.end();
}

ast::TraitDeclPtr TraitChecker::getTrait(const std::string& traitName) const {
    auto it = traits_.find(traitName);
    return it != traits_.end() ? it->second : nullptr;
}

bool TraitChecker::checkTraitImplementation(ast::TypePtr type, const std::string& traitName) {
    if (!type || traitName.empty()) return false;
    
    auto traitDecl = getTrait(traitName);
    if (!traitDecl) {
        reportTraitError("Trait '" + traitName + "' not found");
        return false;
    }
    
    // Check if all required methods are implemented
    auto missingMethods = getMissingMethods(type, traitName);
    if (!missingMethods.empty()) {
        std::string missing = "";
        for (const auto& method : missingMethods) {
            if (!missing.empty()) missing += ", ";
            missing += method;
        }
        reportTraitError("Type '" + type->toString() + "' missing required methods for trait '" + 
                        traitName + "': " + missing);
        return false;
    }
    
    return true;
}

bool TraitChecker::checkTraitImplementation(ast::ClassDeclPtr classDecl, const std::string& traitName) {
    if (!classDecl) return false;
    
    // Create a type for the class and check trait implementation
    auto classType = std::make_shared<ast::SimpleType>(
        lexer::Token(lexer::TokenType::IDENTIFIER, classDecl->getName(), "", 0, 0));
    
    return checkTraitImplementation(classType, traitName);
}

std::vector<std::string> TraitChecker::getMissingMethods(ast::TypePtr type, const std::string& traitName) {
    std::vector<std::string> missing;
    
    auto traitDecl = getTrait(traitName);
    if (!traitDecl) {
        missing.push_back("trait not found");
        return missing;
    }
    
    // Get required methods from trait
    auto requiredMethods = traitDecl->getRequiredMethods();
    
    // Check if type has all required methods
    for (const auto& methodName : requiredMethods) {
        if (!hasRequiredMethod(type, methodName, nullptr)) {
            missing.push_back(methodName);
        }
    }
    
    return missing;
}

bool TraitChecker::hasRequiredMethod(ast::TypePtr type, const std::string& methodName, ast::TypePtr methodType) {
    if (!type) return false;
    
    // Check if the type has the required method
    auto typeKey = type->toString();
    
    // Check built-in methods that all types should have
    if (methodName == "toString" || methodName == "equals" || methodName == "hashCode") {
        return true;
    }
    
    // Check if the type has a method with the given name
    auto methodMap = getTypeMethods(typeKey);
    if (methodMap.find(methodName) != methodMap.end()) {
        // If methodType is provided, check signature compatibility
        if (methodType) {
            auto existingMethod = methodMap[methodName];
            return checkMethodSignatureCompatibility(existingMethod, methodType);
        }
        return true;
    }
    
    // Check if the type implements any traits that provide this method
    auto implementedTraits = getImplementedTraits(type);
    for (const auto& traitName : implementedTraits) {
        auto traitDef = getTrait(traitName);
        if (traitDef && traitDef->hasMethod(methodName)) {
            return true;
        }
    }
    
    return false;
}

bool TraitChecker::checkTraitBounds(ast::TypePtr type, const std::vector<std::string>& requiredTraits) {
    for (const auto& traitName : requiredTraits) {
        if (!satisfiesTraitBound(type, traitName)) {
            return false;
        }
    }
    return true;
}

bool TraitChecker::satisfiesTraitBound(ast::TypePtr type, const std::string& traitName) {
    return checkTraitImplementation(type, traitName);
}

bool TraitChecker::checkTraitInheritance(const std::string& derivedTrait, const std::string& baseTrait) {
    // Check if derivedTrait inherits from baseTrait
    auto hierarchy = getTraitHierarchy(derivedTrait);
    return std::find(hierarchy.begin(), hierarchy.end(), baseTrait) != hierarchy.end();
}

std::vector<std::string> TraitChecker::getTraitHierarchy(const std::string& traitName) {
    std::vector<std::string> hierarchy;
    
    auto traitDecl = getTrait(traitName);
    if (!traitDecl) return hierarchy;
    
    // Add the trait itself
    hierarchy.push_back(traitName);
    
    // Add super traits
    auto superTraits = traitDecl->getSuperTraits();
    for (const auto& superTrait : superTraits) {
        auto superHierarchy = getTraitHierarchy(superTrait);
        hierarchy.insert(hierarchy.end(), superHierarchy.begin(), superHierarchy.end());
    }
    
    return hierarchy;
}

bool TraitChecker::checkAssociatedTypes(ast::TypePtr type, const std::string& traitName) {
    // This would check if the type properly implements associated types
    // For now, return true as placeholder
    return true;
}

ast::TypePtr TraitChecker::resolveAssociatedType(ast::TypePtr type, const std::string& traitName, 
                                                const std::string& associatedTypeName) {
    // This would resolve the associated type for the given type and trait
    // For now, return nullptr as placeholder
    return nullptr;
}

bool TraitChecker::hasDefaultImplementation(const std::string& traitName, const std::string& methodName) {
    auto traitDecl = getTrait(traitName);
    if (!traitDecl) return false;
    
    // Check if the trait has a default implementation for the method
    auto methodDef = traitDecl->getMethod(methodName);
    if (methodDef) {
        return methodDef->hasDefaultImpl;
    }
    
    return false;
}

ast::FunctionDeclPtr TraitChecker::getDefaultImplementation(const std::string& traitName, const std::string& methodName) {
    // This would return the default implementation if it exists
    return nullptr; // Placeholder
}

bool TraitChecker::canCreateTraitObject(const std::string& traitName) {
    auto traitDecl = getTrait(traitName);
    if (!traitDecl) return false;
    
    // Check if the trait is object-safe
    // This would check if all methods are object-safe
    return true; // Placeholder
}

ast::TypePtr TraitChecker::createTraitObjectType(const std::string& traitName) {
    if (!canCreateTraitObject(traitName)) return nullptr;
    
    // Create a trait object type
    return std::make_shared<ast::SimpleType>(
        lexer::Token(lexer::TokenType::IDENTIFIER, "TraitObject<" + traitName + ">", "", 0, 0));
}

void TraitChecker::reportTraitError(const std::string& message, ast::Node* node) {
    errorHandler_.reportError(error::ErrorCode::T017_INVALID_TRAIT_IMPLEMENTATION, message);
}

// Helper method implementations
std::unordered_map<std::string, ast::TypePtr> TraitChecker::getTypeMethods(const std::string& typeKey) const {
    auto it = typeMethods_.find(typeKey);
    if (it != typeMethods_.end()) {
        return it->second;
    }
    return std::unordered_map<std::string, ast::TypePtr>();
}

bool TraitChecker::checkMethodSignatureCompatibility(ast::TypePtr method1, ast::TypePtr method2) const {
    if (!method1 || !method2) return false;
    
    // For now, do a simple string comparison
    // In a real implementation, you would check parameter types, return types, etc.
    return method1->toString() == method2->toString();
}

std::vector<std::string> TraitChecker::getImplementedTraits(ast::TypePtr type) const {
    if (!type) return {};
    
    auto typeKey = type->toString();
    auto it = implementedTraits_.find(typeKey);
    if (it != implementedTraits_.end()) {
        return std::vector<std::string>(it->second.begin(), it->second.end());
    }
    return {};
}

// TraitRegistry implementation
TraitRegistry& TraitRegistry::getInstance() {
    static TraitRegistry instance;
    return instance;
}

bool TraitRegistry::registerTraitDefinition(const TraitDefinition& traitDef) {
    if (traitDef.name.empty()) return false;
    
    if (hasTraitDefinition(traitDef.name)) {
        return false; // Already registered
    }
    
    traitDefinitions_[traitDef.name] = traitDef;
    return true;
}

bool TraitRegistry::hasTraitDefinition(const std::string& traitName) const {
    return traitDefinitions_.find(traitName) != traitDefinitions_.end();
}

const TraitDefinition* TraitRegistry::getTraitDefinition(const std::string& traitName) const {
    auto it = traitDefinitions_.find(traitName);
    return it != traitDefinitions_.end() ? &it->second : nullptr;
}

std::vector<std::string> TraitRegistry::getAllTraitNames() const {
    std::vector<std::string> names;
    names.reserve(traitDefinitions_.size());
    for (const auto& pair : traitDefinitions_) {
        names.push_back(pair.first);
    }
    return names;
}

bool TraitRegistry::registerTraitImplementation(const TraitImplementation& impl) {
    if (impl.traitName.empty() || !impl.implementingType) return false;
    
    std::string typeKey = impl.implementingType->toString();
    traitImplementations_[typeKey].push_back(impl);
    typeTraitMap_[typeKey].insert(impl.traitName);
    
    return true;
}

bool TraitRegistry::hasTraitImplementation(ast::TypePtr type, const std::string& traitName) const {
    if (!type) return false;
    
    std::string typeKey = type->toString();
    auto it = typeTraitMap_.find(typeKey);
    return it != typeTraitMap_.end() && it->second.find(traitName) != it->second.end();
}

const TraitImplementation* TraitRegistry::getTraitImplementation(ast::TypePtr type, const std::string& traitName) const {
    if (!type) return nullptr;
    
    std::string typeKey = type->toString();
    auto it = traitImplementations_.find(typeKey);
    if (it == traitImplementations_.end()) return nullptr;
    
    for (const auto& impl : it->second) {
        if (impl.traitName == traitName) {
            return &impl;
        }
    }
    
    return nullptr;
}

std::vector<std::string> TraitRegistry::getImplementedTraits(ast::TypePtr type) const {
    if (!type) return {};
    
    std::string typeKey = type->toString();
    auto it = typeTraitMap_.find(typeKey);
    if (it == typeTraitMap_.end()) return {};
    
    return std::vector<std::string>(it->second.begin(), it->second.end());
}

bool TraitRegistry::canImplementTrait(ast::TypePtr type, const std::string& traitName) const {
    if (!type) return false;
    
    auto traitDef = getTraitDefinition(traitName);
    if (!traitDef) return false;
    
    // Check if all required methods can be implemented
    auto missing = getMissingRequirements(type, traitName);
    return missing.empty();
}

std::vector<std::string> TraitRegistry::getMissingRequirements(ast::TypePtr type, const std::string& traitName) const {
    std::vector<std::string> missing;
    
    auto traitDef = getTraitDefinition(traitName);
    if (!traitDef) {
        missing.push_back("trait not found");
        return missing;
    }
    
    // Check for missing method implementations
    for (const auto& method : traitDef->methods) {
        if (!method.hasDefaultImpl) {
            // Check if type has this method
            // This is a simplified check
            if (method.name != "toString" && method.name != "equals") {
                missing.push_back(method.name);
            }
        }
    }
    
    return missing;
}

bool TraitRegistry::satisfiesTraitBounds(ast::TypePtr type, const std::vector<std::string>& bounds) const {
    for (const auto& bound : bounds) {
        if (!hasTraitImplementation(type, bound)) {
            return false;
        }
    }
    return true;
}

std::vector<std::string> TraitRegistry::getUnsatisfiedBounds(ast::TypePtr type, const std::vector<std::string>& bounds) const {
    std::vector<std::string> unsatisfied;
    
    for (const auto& bound : bounds) {
        if (!hasTraitImplementation(type, bound)) {
            unsatisfied.push_back(bound);
        }
    }
    
    return unsatisfied;
}

bool TraitRegistry::isGenericTrait(const std::string& traitName) const {
    auto traitDef = getTraitDefinition(traitName);
    return traitDef && !traitDef->typeParameters.empty();
}

std::vector<std::string> TraitRegistry::getTraitTypeParameters(const std::string& traitName) const {
    auto traitDef = getTraitDefinition(traitName);
    return traitDef ? traitDef->typeParameters : std::vector<std::string>{};
}

bool TraitRegistry::isSubTrait(const std::string& subTrait, const std::string& superTrait) const {
    auto traitDef = getTraitDefinition(subTrait);
    if (!traitDef) return false;
    
    return std::find(traitDef->superTraits.begin(), traitDef->superTraits.end(), superTrait) != traitDef->superTraits.end();
}

std::vector<std::string> TraitRegistry::getSuperTraits(const std::string& traitName) const {
    auto traitDef = getTraitDefinition(traitName);
    return traitDef ? traitDef->superTraits : std::vector<std::string>{};
}

std::vector<std::string> TraitRegistry::getSubTraits(const std::string& traitName) const {
    std::vector<std::string> subTraits;
    
    for (const auto& pair : traitDefinitions_) {
        if (isSubTrait(pair.first, traitName)) {
            subTraits.push_back(pair.first);
        }
    }
    
    return subTraits;
}

bool TraitRegistry::isObjectSafe(const std::string& traitName) const {
    auto traitDef = getTraitDefinition(traitName);
    return traitDef && traitDef->isObjectSafe;
}

std::vector<std::string> TraitRegistry::getObjectSafetyViolations(const std::string& traitName) const {
    std::vector<std::string> violations;
    
    auto traitDef = getTraitDefinition(traitName);
    if (!traitDef) {
        violations.push_back("trait not found");
        return violations;
    }
    
    // Check for object safety violations
    // This would check for methods that are not object-safe
    for (const auto& method : traitDef->methods) {
        if (!TraitUtils::isMethodObjectSafe(method)) {
            violations.push_back("method '" + method.name + "' is not object-safe");
        }
    }
    
    return violations;
}

void TraitRegistry::clear() {
    traitDefinitions_.clear();
    traitImplementations_.clear();
    typeTraitMap_.clear();
}

// BuiltinTraits implementation
void BuiltinTraits::registerBuiltinTraits(TraitRegistry& registry) {
    registry.registerTraitDefinition(createCloneTrait());
    registry.registerTraitDefinition(createCopyTrait());
    registry.registerTraitDefinition(createDropTrait());
    registry.registerTraitDefinition(createDebugTrait());
    registry.registerTraitDefinition(createDisplayTrait());
    registry.registerTraitDefinition(createDefaultTrait());
    registry.registerTraitDefinition(createEqTrait());
    registry.registerTraitDefinition(createOrdTrait());
    registry.registerTraitDefinition(createPartialEqTrait());
    registry.registerTraitDefinition(createPartialOrdTrait());
}

TraitDefinition BuiltinTraits::createCloneTrait() {
    TraitDefinition trait;
    trait.name = "Clone";
    trait.isObjectSafe = true;
    
    TraitDefinition::Method cloneMethod;
    cloneMethod.name = "clone";
    cloneMethod.hasDefaultImpl = false;
    trait.methods.push_back(cloneMethod);
    
    return trait;
}

TraitDefinition BuiltinTraits::createCopyTrait() {
    TraitDefinition trait;
    trait.name = "Copy";
    trait.isObjectSafe = true;
    return trait;
}

TraitDefinition BuiltinTraits::createDropTrait() {
    TraitDefinition trait;
    trait.name = "Drop";
    trait.isObjectSafe = true;
    
    TraitDefinition::Method dropMethod;
    dropMethod.name = "drop";
    dropMethod.hasDefaultImpl = false;
    trait.methods.push_back(dropMethod);
    
    return trait;
}

TraitDefinition BuiltinTraits::createDebugTrait() {
    TraitDefinition trait;
    trait.name = "Debug";
    trait.isObjectSafe = true;
    
    TraitDefinition::Method debugMethod;
    debugMethod.name = "debug";
    debugMethod.hasDefaultImpl = false;
    trait.methods.push_back(debugMethod);
    
    return trait;
}

TraitDefinition BuiltinTraits::createDisplayTrait() {
    TraitDefinition trait;
    trait.name = "Display";
    trait.isObjectSafe = true;
    
    TraitDefinition::Method displayMethod;
    displayMethod.name = "display";
    displayMethod.hasDefaultImpl = false;
    trait.methods.push_back(displayMethod);
    
    return trait;
}

TraitDefinition BuiltinTraits::createDefaultTrait() {
    TraitDefinition trait;
    trait.name = "Default";
    trait.isObjectSafe = true;
    
    TraitDefinition::Method defaultMethod;
    defaultMethod.name = "default";
    defaultMethod.hasDefaultImpl = false;
    trait.methods.push_back(defaultMethod);
    
    return trait;
}

TraitDefinition BuiltinTraits::createEqTrait() {
    TraitDefinition trait;
    trait.name = "Eq";
    trait.isObjectSafe = true;
    
    TraitDefinition::Method eqMethod;
    eqMethod.name = "eq";
    eqMethod.hasDefaultImpl = false;
    trait.methods.push_back(eqMethod);
    
    return trait;
}

TraitDefinition BuiltinTraits::createOrdTrait() {
    TraitDefinition trait;
    trait.name = "Ord";
    trait.isObjectSafe = true;
    
    TraitDefinition::Method cmpMethod;
    cmpMethod.name = "cmp";
    cmpMethod.hasDefaultImpl = false;
    trait.methods.push_back(cmpMethod);
    
    return trait;
}

TraitDefinition BuiltinTraits::createPartialEqTrait() {
    TraitDefinition trait;
    trait.name = "PartialEq";
    trait.isObjectSafe = true;
    
    TraitDefinition::Method eqMethod;
    eqMethod.name = "eq";
    eqMethod.hasDefaultImpl = false;
    trait.methods.push_back(eqMethod);
    
    return trait;
}

TraitDefinition BuiltinTraits::createPartialOrdTrait() {
    TraitDefinition trait;
    trait.name = "PartialOrd";
    trait.isObjectSafe = true;
    
    TraitDefinition::Method partialCmpMethod;
    partialCmpMethod.name = "partial_cmp";
    partialCmpMethod.hasDefaultImpl = false;
    trait.methods.push_back(partialCmpMethod);
    
    return trait;
}

// TraitUtils implementation
std::string TraitUtils::mangleTraitName(const std::string& traitName, const std::vector<ast::TypePtr>& typeArgs) {
    std::string mangled = traitName;
    if (!typeArgs.empty()) {
        mangled += "<";
        for (size_t i = 0; i < typeArgs.size(); ++i) {
            if (i > 0) mangled += ",";
            mangled += typeArgs[i]->toString();
        }
        mangled += ">";
    }
    return mangled;
}

bool TraitUtils::isValidTraitName(const std::string& name) {
    if (name.empty()) return false;
    
    // Check for valid identifier
    if (!std::isalpha(name[0]) && name[0] != '_') return false;
    
    for (char c : name) {
        if (!std::isalnum(c) && c != '_') return false;
    }
    
    return true;
}

std::string TraitUtils::normalizeTraitName(const std::string& name) {
    std::string normalized = name;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::tolower);
    return normalized;
}

bool TraitUtils::areMethodSignaturesCompatible(ast::TypePtr sig1, ast::TypePtr sig2) {
    if (!sig1 || !sig2) return false;
    return sig1->toString() == sig2->toString();
}

std::string TraitUtils::getMethodSignatureString(ast::TypePtr signature) {
    return signature ? signature->toString() : "";
}

bool TraitUtils::isMethodObjectSafe(const TraitDefinition::Method& method) {
    // A method is object-safe if it doesn't use Self in its signature
    // This is a simplified check
    return true; // Placeholder
}

bool TraitUtils::areTypeParametersCompatible(const std::vector<std::string>& params1, 
                                            const std::vector<std::string>& params2) {
    if (params1.size() != params2.size()) return false;
    
    for (size_t i = 0; i < params1.size(); ++i) {
        if (params1[i] != params2[i]) return false;
    }
    
    return true;
}

std::string TraitUtils::substituteTypeParameters(const std::string& type, 
                                               const std::unordered_map<std::string, std::string>& substitutions) {
    std::string result = type;
    for (const auto& pair : substitutions) {
        // Simple string replacement
        size_t pos = 0;
        while ((pos = result.find(pair.first, pos)) != std::string::npos) {
            result.replace(pos, pair.first.length(), pair.second);
            pos += pair.second.length();
        }
    }
    return result;
}

std::vector<std::string> TraitUtils::parseTraitBounds(const std::string& boundsStr) {
    std::vector<std::string> bounds;
    std::istringstream iss(boundsStr);
    std::string bound;
    
    while (std::getline(iss, bound, '+')) {
        // Trim whitespace
        bound.erase(0, bound.find_first_not_of(" \t"));
        bound.erase(bound.find_last_not_of(" \t") + 1);
        if (!bound.empty()) {
            bounds.push_back(bound);
        }
    }
    
    return bounds;
}

std::string TraitUtils::formatTraitBounds(const std::vector<std::string>& bounds) {
    std::string result;
    for (size_t i = 0; i < bounds.size(); ++i) {
        if (i > 0) result += " + ";
        result += bounds[i];
    }
    return result;
}

bool TraitUtils::isTraitBoundSatisfied(ast::TypePtr type, const std::string& bound, TraitRegistry& registry) {
    return registry.hasTraitImplementation(type, bound);
}

bool TraitUtils::isTraitImplementationComplete(const TraitImplementation& impl, const TraitDefinition& traitDef) {
    // Check if all required methods are implemented
    for (const auto& method : traitDef.methods) {
        if (!method.hasDefaultImpl) {
            bool found = false;
            for (const auto& implMethod : impl.methodImpls) {
                if (implMethod.name == method.name) {
                    found = true;
                    break;
                }
            }
            if (!found) return false;
        }
    }
    
    return true;
}

std::vector<std::string> TraitUtils::getMissingMethodImplementations(const TraitImplementation& impl, 
                                                                     const TraitDefinition& traitDef) {
    std::vector<std::string> missing;
    
    for (const auto& method : traitDef.methods) {
        if (!method.hasDefaultImpl) {
            bool found = false;
            for (const auto& implMethod : impl.methodImpls) {
                if (implMethod.name == method.name) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                missing.push_back(method.name);
            }
        }
    }
    
    return missing;
}

std::vector<std::string> TraitUtils::getMissingAssociatedTypes(const TraitImplementation& impl, 
                                                               const TraitDefinition& traitDef) {
    std::vector<std::string> missing;
    
    for (const auto& associatedType : traitDef.associatedTypes) {
        bool found = false;
        for (const auto& implType : impl.associatedTypeImpls) {
            if (implType.name == associatedType.name) {
                found = true;
                break;
            }
        }
        if (!found) {
            missing.push_back(associatedType.name);
        }
    }
    
    return missing;
}

std::string TraitUtils::formatTraitError(const std::string& traitName, const std::string& typeName, 
                                         const std::string& issue) {
    return "Trait '" + traitName + "' implementation for type '" + typeName + "': " + issue;
}

std::string TraitUtils::formatMissingMethodError(const std::string& traitName, const std::string& methodName, 
                                                 const std::string& typeName) {
    return "Type '" + typeName + "' missing required method '" + methodName + "' for trait '" + traitName + "'";
}

// TraitConstraints implementation
TraitConstraints::TraitConstraints() {
}

TraitConstraints::~TraitConstraints() {
}

void TraitConstraints::addConstraint(const Constraint& constraint) {
    constraints_[constraint.typeParameter] = constraint;
}

void TraitConstraints::removeConstraint(const std::string& typeParameter) {
    constraints_.erase(typeParameter);
}

bool TraitConstraints::hasConstraint(const std::string& typeParameter) const {
    return constraints_.find(typeParameter) != constraints_.end();
}

const TraitConstraints::Constraint* TraitConstraints::getConstraint(const std::string& typeParameter) const {
    auto it = constraints_.find(typeParameter);
    return it != constraints_.end() ? &it->second : nullptr;
}

bool TraitConstraints::checkConstraints(const std::unordered_map<std::string, ast::TypePtr>& typeBindings, 
                                       TraitRegistry& registry) const {
    for (const auto& pair : constraints_) {
        const auto& constraint = pair.second;
        auto it = typeBindings.find(constraint.typeParameter);
        if (it == typeBindings.end()) continue;
        
        if (!constraint.isSatisfiedBy(it->second, registry)) {
            return false;
        }
    }
    return true;
}

std::vector<std::string> TraitConstraints::getViolatedConstraints(const std::unordered_map<std::string, ast::TypePtr>& typeBindings, 
                                                                 TraitRegistry& registry) const {
    std::vector<std::string> violated;
    
    for (const auto& pair : constraints_) {
        const auto& constraint = pair.second;
        auto it = typeBindings.find(constraint.typeParameter);
        if (it == typeBindings.end()) continue;
        
        if (!constraint.isSatisfiedBy(it->second, registry)) {
            violated.push_back(constraint.typeParameter);
        }
    }
    
    return violated;
}

bool TraitConstraints::inferConstraints(const std::vector<ast::TypePtr>& types, TraitRegistry& registry) {
    // This would infer constraints based on type usage
    // For now, return true as placeholder
    return true;
}

std::vector<TraitConstraints::Constraint> TraitConstraints::getInferredConstraints() const {
    std::vector<Constraint> inferred;
    for (const auto& pair : constraints_) {
        inferred.push_back(pair.second);
    }
    return inferred;
}

bool TraitConstraints::Constraint::isSatisfiedBy(ast::TypePtr type, TraitRegistry& registry) const {
    // Check required traits
    for (const auto& requiredTrait : requiredTraits) {
        if (!registry.hasTraitImplementation(type, requiredTrait)) {
            return false;
        }
    }
    
    // Check excluded traits
    for (const auto& excludedTrait : excludedTraits) {
        if (registry.hasTraitImplementation(type, excludedTrait)) {
            return false;
        }
    }
    
    return true;
}

} // namespace type_checker 