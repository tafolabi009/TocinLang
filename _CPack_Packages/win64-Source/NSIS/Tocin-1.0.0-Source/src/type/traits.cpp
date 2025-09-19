#include "traits.h"

namespace type {

std::shared_ptr<TraitRegistry> global_trait_registry;

void initializeTraitRegistry() {
    if (!global_trait_registry) {
        global_trait_registry = std::make_shared<TraitRegistry>();
    }
}

TraitRegistry& getTraitRegistry() {
    initializeTraitRegistry();
    return *global_trait_registry;
}

std::shared_ptr<Trait> createTrait(const std::string& name) {
    return std::make_shared<Trait>(name);
}

std::shared_ptr<TraitImpl> createTraitImpl(const std::string& trait_name, const std::string& type_name) {
    return std::make_shared<TraitImpl>(trait_name, type_name);
}

std::shared_ptr<TraitBound> createTraitBound(const std::string& type_name) {
    return std::make_shared<TraitBound>(type_name);
}

std::shared_ptr<TypeConstraint> createTypeConstraint(const std::string& trait_name) {
    return std::make_shared<TypeConstraint>(trait_name);
}

} // namespace type

namespace type_checker {

TraitChecker::TraitChecker(error::ErrorHandler& errorHandler)
    : errorHandler_(errorHandler) {
}

TraitChecker::~TraitChecker() = default;

} // namespace type_checker

