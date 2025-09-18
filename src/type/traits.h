#ifndef TOCIN_TRAITS_H
#define TOCIN_TRAITS_H

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <mutex>
<<<<<<< Current (Your changes)
=======
#include <unordered_set>
>>>>>>> Incoming (Background Agent changes)
#include <functional>
#include <type_traits>
#include <variant>
<<<<<<< Current (Your changes)
#include <mutex>
#include <algorithm>
=======
#include <unordered_set>
#include <algorithm>
#include <mutex>

#include "../ast/ast.h"
#include "../ast/types.h"
#include "../lexer/token.h"
#include "../error/error_handler.h"
>>>>>>> Incoming (Background Agent changes)

namespace type {

// Forward declarations
class Trait;
class TraitImpl;
class TypeConstraint;
class TraitBound;

/**
 * @brief Trait method signature
 */
struct TraitMethod {
    std::string name;
    std::vector<std::string> parameter_types;
    std::string return_type;
    bool is_default = false;
    std::string default_implementation;
    std::string documentation;
};

/**
 * @brief Associated type definition
 */
struct AssociatedType {
    std::string name;
    std::string default_type;
    std::vector<std::string> constraints;
    std::string documentation;
};

/**
 * @brief Trait definition
 */
class Trait {
private:
    std::string name;
    std::vector<TraitMethod> methods;
    std::vector<AssociatedType> associated_types;
    std::vector<std::string> super_traits;
    std::string documentation;
    bool is_marker = false;
    bool is_auto = false;

public:
    Trait(const std::string& n) : name(n) {}

    /**
     * @brief Get trait name
     */
    const std::string& getName() const { return name; }

    /**
     * @brief Add a method to the trait
     */
    void addMethod(const TraitMethod& method) {
        methods.push_back(method);
    }

    /**
     * @brief Add an associated type
     */
    void addAssociatedType(const AssociatedType& type) {
        associated_types.push_back(type);
    }

    /**
     * @brief Add a super trait
     */
    void addSuperTrait(const std::string& super_trait) {
        super_traits.push_back(super_trait);
    }

    /**
     * @brief Get all methods
     */
    const std::vector<TraitMethod>& getMethods() const { return methods; }

    /**
     * @brief Get all associated types
     */
    const std::vector<AssociatedType>& getAssociatedTypes() const { return associated_types; }

    /**
     * @brief Get super traits
     */
    const std::vector<std::string>& getSuperTraits() const { return super_traits; }

    /**
     * @brief Check if trait is a marker trait
     */
    bool isMarkerTrait() const { return is_marker; }

    /**
     * @brief Set marker trait status
     */
    void setMarkerTrait(bool marker) { is_marker = marker; }

    /**
     * @brief Check if trait is auto-implemented
     */
    bool isAutoTrait() const { return is_auto; }

    /**
     * @brief Set auto trait status
     */
    void setAutoTrait(bool auto_trait) { is_auto = auto_trait; }

    /**
     * @brief Get documentation
     */
    const std::string& getDocumentation() const { return documentation; }

    /**
     * @brief Set documentation
     */
    void setDocumentation(const std::string& doc) { documentation = doc; }

    /**
     * @brief Check if trait has a specific method
     */
    bool hasMethod(const std::string& method_name) const {
        for (const auto& method : methods) {
            if (method.name == method_name) return true;
        }
        return false;
    }

    /**
     * @brief Get a specific method
     */
    const TraitMethod* getMethod(const std::string& method_name) const {
        for (const auto& method : methods) {
            if (method.name == method_name) return &method;
        }
        return nullptr;
    }
};

/**
 * @brief Type constraint for trait bounds
 */
class TypeConstraint {
private:
    std::string trait_name;
    std::vector<std::string> type_parameters;
    std::vector<std::string> lifetime_parameters;

public:
    TypeConstraint(const std::string& trait, const std::vector<std::string>& params = {})
        : trait_name(trait), type_parameters(params) {}

    /**
     * @brief Get trait name
     */
    const std::string& getTraitName() const { return trait_name; }

    /**
     * @brief Get type parameters
     */
    const std::vector<std::string>& getTypeParameters() const { return type_parameters; }

    /**
     * @brief Add type parameter
     */
    void addTypeParameter(const std::string& param) {
        type_parameters.push_back(param);
    }

    /**
     * @brief Add lifetime parameter
     */
    void addLifetimeParameter(const std::string& lifetime) {
        lifetime_parameters.push_back(lifetime);
    }

    /**
     * @brief Get lifetime parameters
     */
    const std::vector<std::string>& getLifetimeParameters() const { return lifetime_parameters; }

    /**
     * @brief Convert to string representation
     */
    std::string toString() const {
        std::string result = trait_name;
        if (!type_parameters.empty()) {
            result += "<";
            for (size_t i = 0; i < type_parameters.size(); ++i) {
                if (i > 0) result += ", ";
                result += type_parameters[i];
            }
            result += ">";
        }
        return result;
    }
};

/**
 * @brief Trait bound for type parameters
 */
class TraitBound {
private:
    std::vector<TypeConstraint> constraints;
    std::string bound_type;

public:
    TraitBound(const std::string& type) : bound_type(type) {}

    /**
     * @brief Add a constraint
     */
    void addConstraint(const TypeConstraint& constraint) {
        constraints.push_back(constraint);
    }

    /**
     * @brief Get all constraints
     */
    const std::vector<TypeConstraint>& getConstraints() const { return constraints; }

    /**
     * @brief Get bound type
     */
    const std::string& getBoundType() const { return bound_type; }

    /**
     * @brief Convert to string representation
     */
    std::string toString() const {
        std::string result = bound_type;
        if (!constraints.empty()) {
            result += ": ";
            for (size_t i = 0; i < constraints.size(); ++i) {
                if (i > 0) result += " + ";
                result += constraints[i].toString();
            }
        }
        return result;
    }
};

/**
 * @brief Trait implementation for a specific type
 */
class TraitImpl {
private:
    std::string trait_name;
    std::string type_name;
    std::unordered_map<std::string, std::string> method_implementations;
    std::unordered_map<std::string, std::string> associated_type_values;
    std::vector<std::string> where_clauses;

public:
    TraitImpl(const std::string& trait, const std::string& type)
        : trait_name(trait), type_name(type) {}

    /**
     * @brief Get trait name
     */
    const std::string& getTraitName() const { return trait_name; }

    /**
     * @brief Get type name
     */
    const std::string& getTypeName() const { return type_name; }

    /**
     * @brief Add method implementation
     */
    void addMethodImplementation(const std::string& method_name, const std::string& implementation) {
        method_implementations[method_name] = implementation;
    }

    /**
     * @brief Add associated type value
     */
    void addAssociatedTypeValue(const std::string& type_name, const std::string& value) {
        associated_type_values[type_name] = value;
    }

    /**
     * @brief Add where clause
     */
    void addWhereClause(const std::string& clause) {
        where_clauses.push_back(clause);
    }

    /**
     * @brief Get method implementation
     */
    const std::string* getMethodImplementation(const std::string& method_name) const {
        auto it = method_implementations.find(method_name);
        return it != method_implementations.end() ? &it->second : nullptr;
    }

    /**
     * @brief Get associated type value
     */
    const std::string* getAssociatedTypeValue(const std::string& type_name) const {
        auto it = associated_type_values.find(type_name);
        return it != associated_type_values.end() ? &it->second : nullptr;
    }

    /**
     * @brief Get where clauses
     */
    const std::vector<std::string>& getWhereClauses() const { return where_clauses; }

    /**
     * @brief Check if implementation is complete
     */
    bool isComplete(const Trait& trait) const {
        for (const auto& method : trait.getMethods()) {
            if (!method.is_default && !getMethodImplementation(method.name)) {
                return false;
            }
        }
        return true;
    }
};

/**
 * @brief Trait registry for managing all traits
 */
class TraitRegistry {
private:
    std::unordered_map<std::string, std::shared_ptr<Trait>> traits;
    std::unordered_map<std::string, std::vector<std::shared_ptr<TraitImpl>>> implementations;
    mutable std::mutex registry_mutex;

public:
    /**
     * @brief Register a trait
     */
    void registerTrait(std::shared_ptr<Trait> trait) {
        std::lock_guard<std::mutex> lock(registry_mutex);
        traits[trait->getName()] = trait;
    }

    /**
     * @brief Register a trait implementation
     */
    void registerImplementation(std::shared_ptr<TraitImpl> impl) {
        std::lock_guard<std::mutex> lock(registry_mutex);
        std::string key = impl->getTraitName() + " for " + impl->getTypeName();
        implementations[key].push_back(impl);
    }

    /**
     * @brief Get a trait by name
     */
    std::shared_ptr<Trait> getTrait(const std::string& name) const {
        std::lock_guard<std::mutex> lock(registry_mutex);
        auto it = traits.find(name);
        return it != traits.end() ? it->second : nullptr;
    }

    /**
     * @brief Get trait implementation
     */
    std::shared_ptr<TraitImpl> getImplementation(const std::string& trait_name, 
                                                const std::string& type_name) const {
        std::lock_guard<std::mutex> lock(registry_mutex);
        std::string key = trait_name + " for " + type_name;
        auto it = implementations.find(key);
        if (it != implementations.end() && !it->second.empty()) {
            return it->second.front();
        }
        return nullptr;
    }

    /**
     * @brief Check if type implements trait
     */
    bool typeImplementsTrait(const std::string& type_name, const std::string& trait_name) const {
        return getImplementation(trait_name, type_name) != nullptr;
    }

    /**
     * @brief Get all traits
     */
    std::vector<std::shared_ptr<Trait>> getAllTraits() const {
        std::lock_guard<std::mutex> lock(registry_mutex);
        std::vector<std::shared_ptr<Trait>> result;
        for (const auto& pair : traits) {
            result.push_back(pair.second);
        }
        return result;
    }

    /**
     * @brief Get all implementations for a trait
     */
    std::vector<std::shared_ptr<TraitImpl>> getImplementationsForTrait(const std::string& trait_name) const {
        std::lock_guard<std::mutex> lock(registry_mutex);
        std::vector<std::shared_ptr<TraitImpl>> result;
        for (const auto& pair : implementations) {
            if (pair.first.find(trait_name + " for ") == 0) {
                result.insert(result.end(), pair.second.begin(), pair.second.end());
            }
        }
        return result;
    }
};

/**
 * @brief Trait solver for resolving trait bounds
 */
class TraitSolver {
private:
    std::shared_ptr<TraitRegistry> registry;

public:
    explicit TraitSolver(std::shared_ptr<TraitRegistry> reg) : registry(reg) {}

    /**
     * @brief Check if type satisfies trait bound
     */
    bool satisfiesBound(const std::string& type_name, const TraitBound& bound) {
        for (const auto& constraint : bound.getConstraints()) {
            if (!registry->typeImplementsTrait(type_name, constraint.getTraitName())) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Find implementation for trait method
     */
    std::string findMethodImplementation(const std::string& type_name, 
                                       const std::string& trait_name,
                                       const std::string& method_name) {
        auto impl = registry->getImplementation(trait_name, type_name);
        if (impl) {
            auto method_impl = impl->getMethodImplementation(method_name);
            if (method_impl) {
                return *method_impl;
            }
        }

        // Check for default implementation
        auto trait = registry->getTrait(trait_name);
        if (trait) {
            auto method = trait->getMethod(method_name);
            if (method && method->is_default) {
                return method->default_implementation;
            }
        }

        return "";
    }

    /**
     * @brief Resolve associated type
     */
    std::string resolveAssociatedType(const std::string& type_name,
                                     const std::string& trait_name,
                                     const std::string& associated_type_name) {
        auto impl = registry->getImplementation(trait_name, type_name);
        if (impl) {
            auto type_value = impl->getAssociatedTypeValue(associated_type_name);
            if (type_value) {
                return *type_value;
            }
        }

        // Check for default associated type
        auto trait = registry->getTrait(trait_name);
        if (trait) {
            for (const auto& assoc_type : trait->getAssociatedTypes()) {
                if (assoc_type.name == associated_type_name) {
                    return assoc_type.default_type;
                }
            }
        }

        return "";
    }
};

// Global trait registry instance
extern std::shared_ptr<TraitRegistry> global_trait_registry;

/**
 * @brief Initialize the global trait registry
 */
void initializeTraitRegistry();

/**
 * @brief Get the global trait registry
 */
TraitRegistry& getTraitRegistry();

/**
 * @brief Create a new trait
 */
std::shared_ptr<Trait> createTrait(const std::string& name);

/**
 * @brief Create a trait implementation
 */
std::shared_ptr<TraitImpl> createTraitImpl(const std::string& trait_name, const std::string& type_name);

/**
 * @brief Create a trait bound
 */
std::shared_ptr<TraitBound> createTraitBound(const std::string& type_name);

/**
 * @brief Create a type constraint
 */
std::shared_ptr<TypeConstraint> createTypeConstraint(const std::string& trait_name);

} // namespace type

// Declarations for the trait system implemented in traits.cpp
namespace type_checker {

// Lightweight trait model used by the checker/registry in traits.cpp
struct TraitDefinition {
    struct Method {
        std::string name;
        bool hasDefaultImpl = false;
    };

    struct AssociatedTypeDef {
        std::string name;
    };

    std::string name;
    bool isObjectSafe = true;
    std::vector<std::string> typeParameters;
    std::vector<std::string> superTraits;
    std::vector<Method> methods;
    std::vector<AssociatedTypeDef> associatedTypes;
};

struct TraitImplementation {
    std::string traitName;
    ast::TypePtr implementingType;
    struct ImplMethod { std::string name; };
    std::vector<ImplMethod> methodImpls;
    struct AssocTypeImpl { std::string name; };
    std::vector<AssocTypeImpl> associatedTypeImpls;
};

class TraitChecker {
public:
    explicit TraitChecker(error::ErrorHandler& errorHandler);
    ~TraitChecker();

    bool registerTrait(ast::TraitDeclPtr traitDecl);
    bool isTraitRegistered(const std::string& traitName) const;
    ast::TraitDeclPtr getTrait(const std::string& traitName) const;

    bool checkTraitImplementation(ast::TypePtr type, const std::string& traitName);
    bool checkTraitImplementation(ast::ClassDeclPtr classDecl, const std::string& traitName);

    std::vector<std::string> getMissingMethods(ast::TypePtr type, const std::string& traitName);
    bool hasRequiredMethod(ast::TypePtr type, const std::string& methodName, ast::TypePtr methodType);

    bool checkTraitBounds(ast::TypePtr type, const std::vector<std::string>& requiredTraits);
    bool satisfiesTraitBound(ast::TypePtr type, const std::string& traitName);
    bool checkTraitInheritance(const std::string& derivedTrait, const std::string& baseTrait);
    std::vector<std::string> getTraitHierarchy(const std::string& traitName);

    bool checkAssociatedTypes(ast::TypePtr type, const std::string& traitName);
    ast::TypePtr resolveAssociatedType(ast::TypePtr type, const std::string& traitName,
                                       const std::string& associatedTypeName);

    bool hasDefaultImplementation(const std::string& traitName, const std::string& methodName);
    ast::FunctionDeclPtr getDefaultImplementation(const std::string& traitName, const std::string& methodName);

    bool canCreateTraitObject(const std::string& traitName);
    ast::TypePtr createTraitObjectType(const std::string& traitName);

private:
    void reportTraitError(const std::string& message, ast::ASTNodePtr node = nullptr);
    std::unordered_map<std::string, ast::TypePtr> getTypeMethods(const std::string& typeKey) const;
    bool checkMethodSignatureCompatibility(ast::TypePtr method1, ast::TypePtr method2) const;
    std::vector<std::string> getImplementedTraits(ast::TypePtr type) const;

    error::ErrorHandler& errorHandler_;
    std::unordered_map<std::string, ast::TraitDeclPtr> traits_;
    std::unordered_map<std::string, std::unordered_map<std::string, ast::TypePtr>> typeMethods_;
    std::unordered_map<std::string, std::unordered_set<std::string>> implementedTraits_;
};

class TraitRegistry {
public:
    static TraitRegistry& getInstance();

    bool registerTraitDefinition(const TraitDefinition& traitDef);
    bool hasTraitDefinition(const std::string& traitName) const;
    const TraitDefinition* getTraitDefinition(const std::string& traitName) const;
    std::vector<std::string> getAllTraitNames() const;

    bool registerTraitImplementation(const TraitImplementation& impl);
    bool hasTraitImplementation(ast::TypePtr type, const std::string& traitName) const;
    const TraitImplementation* getTraitImplementation(ast::TypePtr type, const std::string& traitName) const;
    std::vector<std::string> getImplementedTraits(ast::TypePtr type) const;
    bool canImplementTrait(ast::TypePtr type, const std::string& traitName) const;
    std::vector<std::string> getMissingRequirements(ast::TypePtr type, const std::string& traitName) const;

    bool satisfiesTraitBounds(ast::TypePtr type, const std::vector<std::string>& bounds) const;
    std::vector<std::string> getUnsatisfiedBounds(ast::TypePtr type, const std::vector<std::string>& bounds) const;

    bool isGenericTrait(const std::string& traitName) const;
    std::vector<std::string> getTraitTypeParameters(const std::string& traitName) const;
    bool isSubTrait(const std::string& subTrait, const std::string& superTrait) const;
    std::vector<std::string> getSuperTraits(const std::string& traitName) const;
    std::vector<std::string> getSubTraits(const std::string& traitName) const;

    bool isObjectSafe(const std::string& traitName) const;
    std::vector<std::string> getObjectSafetyViolations(const std::string& traitName) const;

    void clear();

private:
    std::unordered_map<std::string, TraitDefinition> traitDefinitions_;
    std::unordered_map<std::string, std::vector<TraitImplementation>> traitImplementations_;
    std::unordered_map<std::string, std::unordered_set<std::string>> typeTraitMap_;
};

struct TraitUtils {
    static std::string mangleTraitName(const std::string& traitName, const std::vector<ast::TypePtr>& typeArgs);
    static bool isValidTraitName(const std::string& name);
    static std::string normalizeTraitName(const std::string& name);
    static bool areMethodSignaturesCompatible(ast::TypePtr sig1, ast::TypePtr sig2);
    static std::string getMethodSignatureString(ast::TypePtr signature);
    static bool isMethodObjectSafe(const TraitDefinition::Method& method);
    static bool areTypeParametersCompatible(const std::vector<std::string>& params1,
                                            const std::vector<std::string>& params2);
    static std::string substituteTypeParameters(const std::string& type,
                                                const std::unordered_map<std::string, std::string>& substitutions);
    static std::vector<std::string> parseTraitBounds(const std::string& boundsStr);
    static std::string formatTraitBounds(const std::vector<std::string>& bounds);
    static bool isTraitBoundSatisfied(ast::TypePtr type, const std::string& bound, TraitRegistry& registry);
    static bool isTraitImplementationComplete(const TraitImplementation& impl, const TraitDefinition& traitDef);
    static std::vector<std::string> getMissingMethodImplementations(const TraitImplementation& impl,
                                                                    const TraitDefinition& traitDef);
    static std::vector<std::string> getMissingAssociatedTypes(const TraitImplementation& impl,
                                                              const TraitDefinition& traitDef);
    static std::string formatTraitError(const std::string& traitName, const std::string& typeName,
                                        const std::string& issue);
    static std::string formatMissingMethodError(const std::string& traitName, const std::string& methodName,
                                                const std::string& typeName);
};

class TraitConstraints {
public:
    struct Constraint {
        std::string typeParameter;
        std::vector<std::string> requiredTraits;
        std::vector<std::string> excludedTraits;
        bool isSatisfiedBy(ast::TypePtr type, TraitRegistry& registry) const;
    };

    TraitConstraints();
    ~TraitConstraints();

    void addConstraint(const Constraint& constraint);
    void removeConstraint(const std::string& typeParameter);
    bool hasConstraint(const std::string& typeParameter) const;
    const Constraint* getConstraint(const std::string& typeParameter) const;

    bool checkConstraints(const std::unordered_map<std::string, ast::TypePtr>& typeBindings,
                          TraitRegistry& registry) const;
    std::vector<std::string> getViolatedConstraints(const std::unordered_map<std::string, ast::TypePtr>& typeBindings,
                                                    TraitRegistry& registry) const;
    bool inferConstraints(const std::vector<ast::TypePtr>& types, TraitRegistry& registry);
    std::vector<Constraint> getInferredConstraints() const;

private:
    std::unordered_map<std::string, Constraint> constraints_;
};

} // namespace type_checker

#endif // TOCIN_TRAITS_H