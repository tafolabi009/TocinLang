#pragma once

#include "type.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace type {

/**
 * Represents a method signature within a trait
 */
class TraitMethod {
public:
    TraitMethod(std::string name, 
                TypePtr returnType,
                std::vector<std::pair<std::string, TypePtr>> parameters,
                bool isStatic = false)
        : name(std::move(name)), 
          returnType(std::move(returnType)),
          parameters(std::move(parameters)),
          isStatic(isStatic) {}

    const std::string& getName() const { return name; }
    const TypePtr& getReturnType() const { return returnType; }
    const std::vector<std::pair<std::string, TypePtr>>& getParameters() const { return parameters; }
    bool getIsStatic() const { return isStatic; }

private:
    std::string name;
    TypePtr returnType;
    std::vector<std::pair<std::string, TypePtr>> parameters;
    bool isStatic;
};

/**
 * Represents a trait (similar to Rust traits or TypeScript interfaces)
 * Traits define behavior that types can implement
 */
class Trait {
public:
    explicit Trait(std::string name) : name(std::move(name)) {}

    // Add a method to the trait
    void addMethod(TraitMethod method) {
        methods[method.getName()] = std::move(method);
    }

    // Add an associated type to the trait
    void addAssociatedType(std::string name) {
        associatedTypes.push_back(std::move(name));
    }

    // Add a parent trait (for trait inheritance)
    void addParentTrait(std::shared_ptr<Trait> parent) {
        parentTraits.push_back(std::move(parent));
    }

    // Getters
    const std::string& getName() const { return name; }
    const std::unordered_map<std::string, TraitMethod>& getMethods() const { return methods; }
    const std::vector<std::string>& getAssociatedTypes() const { return associatedTypes; }
    const std::vector<std::shared_ptr<Trait>>& getParentTraits() const { return parentTraits; }

    // Check if this trait has a specific method
    bool hasMethod(const std::string& methodName) const {
        if (methods.find(methodName) != methods.end()) {
            return true;
        }

        // Check parent traits
        for (const auto& parent : parentTraits) {
            if (parent->hasMethod(methodName)) {
                return true;
            }
        }

        return false;
    }

    // Get a method by name (including from parent traits)
    const TraitMethod* getMethod(const std::string& methodName) const {
        auto it = methods.find(methodName);
        if (it != methods.end()) {
            return &it->second;
        }

        // Try to find in parent traits
        for (const auto& parent : parentTraits) {
            const TraitMethod* method = parent->getMethod(methodName);
            if (method) {
                return method;
            }
        }

        return nullptr;
    }

private:
    std::string name;
    std::unordered_map<std::string, TraitMethod> methods;
    std::vector<std::string> associatedTypes;
    std::vector<std::shared_ptr<Trait>> parentTraits;
};

/**
 * Represents a trait implementation for a specific type
 */
class TraitImplementation {
public:
    TraitImplementation(std::shared_ptr<Trait> trait, TypePtr implementingType)
        : trait(std::move(trait)), implementingType(std::move(implementingType)) {}

    // Add an implementation of a method
    void addMethodImplementation(std::string methodName, void* implementationPtr) {
        methodImplementations[std::move(methodName)] = implementationPtr;
    }

    // Add an associated type implementation
    void addAssociatedTypeImplementation(std::string typeName, TypePtr type) {
        associatedTypeImplementations[std::move(typeName)] = std::move(type);
    }

    // Getters
    std::shared_ptr<Trait> getTrait() const { return trait; }
    TypePtr getImplementingType() const { return implementingType; }
    
    // Check if all required methods are implemented
    bool isComplete() const {
        for (const auto& [methodName, method] : trait->getMethods()) {
            if (methodImplementations.find(methodName) == methodImplementations.end()) {
                return false;
            }
        }

        for (const auto& typeName : trait->getAssociatedTypes()) {
            if (associatedTypeImplementations.find(typeName) == associatedTypeImplementations.end()) {
                return false;
            }
        }

        return true;
    }

private:
    std::shared_ptr<Trait> trait;
    TypePtr implementingType;
    std::unordered_map<std::string, void*> methodImplementations;
    std::unordered_map<std::string, TypePtr> associatedTypeImplementations;
};

// Registry for traits and implementations
class TraitRegistry {
public:
    // Register a trait
    void registerTrait(std::shared_ptr<Trait> trait) {
        traits[trait->getName()] = std::move(trait);
    }

    // Register an implementation
    void registerImplementation(std::shared_ptr<TraitImplementation> implementation) {
        implementations.push_back(std::move(implementation));
    }

    // Find a trait by name
    std::shared_ptr<Trait> findTrait(const std::string& name) const {
        auto it = traits.find(name);
        if (it != traits.end()) {
            return it->second;
        }
        return nullptr;
    }

    // Find all implementations for a type
    std::vector<std::shared_ptr<TraitImplementation>> findImplementationsForType(const TypePtr& type) const {
        std::vector<std::shared_ptr<TraitImplementation>> result;
        for (const auto& implementation : implementations) {
            if (implementation->getImplementingType()->equals(type)) {
                result.push_back(implementation);
            }
        }
        return result;
    }

    // Find a specific implementation
    std::shared_ptr<TraitImplementation> findImplementation(const std::string& traitName, const TypePtr& type) const {
        for (const auto& implementation : implementations) {
            if (implementation->getTrait()->getName() == traitName && 
                implementation->getImplementingType()->equals(type)) {
                return implementation;
            }
        }
        return nullptr;
    }

    // Check if a type implements a trait
    bool hasImplementation(const std::string& traitName, const TypePtr& type) const {
        return findImplementation(traitName, type) != nullptr;
    }

private:
    std::unordered_map<std::string, std::shared_ptr<Trait>> traits;
    std::vector<std::shared_ptr<TraitImplementation>> implementations;
};

} // namespace type 
