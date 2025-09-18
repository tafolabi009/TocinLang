#include "feature_integration.h"
#include <iostream>
#include <sstream>
#include "traits.h"

namespace type_checker {

FeatureManager::FeatureManager(error::ErrorHandler& errorHandler)
    : errorHandler_(errorHandler), initialized_(false) {
    initializeFeatureFlags();
}

FeatureManager::~FeatureManager() {
    finalize();
}

bool FeatureManager::initialize() {
    if (initialized_) return true;
    
    try {
        // Initialize all feature components
        ownershipChecker_ = std::make_unique<OwnershipChecker>(errorHandler_);
        resultOptionChecker_ = std::make_unique<ResultOptionMatcher>(errorHandler_);
        nullSafetyChecker_ = std::make_unique<NullSafetyChecker>(errorHandler_);
        extensionFunctionChecker_ = std::make_unique<ExtensionManager>(errorHandler_);
        moveSemanticsChecker_ = std::make_unique<MoveChecker>(errorHandler_, *ownershipChecker_);
        type::initializeTraitRegistry();
        traitChecker_ = std::make_unique<type::TraitSolver>(type::global_trait_registry);
        
        initialized_ = true;
        return true;
    } catch (const std::exception& e) {
        errorHandler_.reportError(error::ErrorCode::G008_INITIALIZATION_ERROR,
                                "Failed to initialize feature manager: " + std::string(e.what()));
        return false;
    }
}

void FeatureManager::finalize() {
    if (!initialized_) return;
    
    // Clean up resources
    typeCache_.clear();
    genericContextStack_.clear();
    initialized_ = false;
}

void FeatureManager::initializeFeatureFlags() {
    // Enable all features by default
    featureFlags_["ownership"] = true;
    featureFlags_["result_option"] = true;
    featureFlags_["null_safety"] = true;
    featureFlags_["extension_functions"] = true;
    featureFlags_["move_semantics"] = true;
    featureFlags_["traits"] = true;
    featureFlags_["async_await"] = true;
    featureFlags_["linq"] = true;
    featureFlags_["pattern_matching"] = true;
    featureFlags_["generics"] = true;
}

bool FeatureManager::checkExpression(ast::ExprPtr expr, ast::TypePtr expectedType) {
    if (!initialized_) return false;
    
    try {
        // Check ownership
        if (isFeatureEnabled("ownership")) {
            if (!ownershipChecker_->checkExpression(expr)) {
                return false;
            }
        }
        
        // Check null safety
        if (isFeatureEnabled("null_safety")) {
            if (!nullSafetyChecker_->checkExpression(expr)) {
                return false;
            }
        }
        
        // Validate move if applicable (non-fatal)
        if (isFeatureEnabled("move_semantics")) {
            TOCIN_UNUSED(moveSemanticsChecker_->validateMove(expr));
        }
        
        return true;
    } catch (const std::exception& e) {
        reportFeatureError("Expression check failed: " + std::string(e.what()));
        return false;
    }
}

bool FeatureManager::checkStatement(ast::StmtPtr stmt) {
    if (!initialized_) return false;
    
    try {
        // Check ownership
        if (isFeatureEnabled("ownership")) {
            if (!ownershipChecker_->checkStatement(stmt)) {
                return false;
            }
        }
        
        // Check null safety
        if (isFeatureEnabled("null_safety")) {
            if (!nullSafetyChecker_->checkStatement(stmt)) {
                return false;
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        reportFeatureError("Statement check failed: " + std::string(e.what()));
        return false;
    }
}

bool FeatureManager::checkFunction(ast::FunctionDeclPtr function) {
    if (!initialized_) return false;
    
    try {
        // Check all features for function
        if (isFeatureEnabled("ownership")) {
            if (!ownershipChecker_->checkFunction(function)) {
                return false;
            }
        }
        
        if (isFeatureEnabled("null_safety")) {
            if (!nullSafetyChecker_->checkFunction(function)) {
                return false;
            }
        }
        
        if (isFeatureEnabled("move_semantics")) {
            if (!moveSemanticsChecker_->checkFunction(function)) {
                return false;
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        reportFeatureError("Function check failed: " + std::string(e.what()));
        return false;
    }
}

bool FeatureManager::checkClass(ast::ClassDeclPtr classDecl) {
    if (!initialized_) return false;
    
    try {
        TOCIN_UNUSED(classDecl);
        
        return true;
    } catch (const std::exception& e) {
        reportFeatureError("Class check failed: " + std::string(e.what()));
        return false;
    }
}

bool FeatureManager::checkTrait(ast::TraitDeclPtr traitDecl) {
    if (!initialized_) return false;
    
    try {
        TOCIN_UNUSED(traitDecl);
        
        return true;
    } catch (const std::exception& e) {
        reportFeatureError("Trait check failed: " + std::string(e.what()));
        return false;
    }
}

ast::TypePtr FeatureManager::resolveType(ast::TypePtr type) {
    if (!type) return nullptr;
    
    // Check cache first
    auto it = typeCache_.find(type->toString());
    if (it != typeCache_.end()) {
        return it->second;
    }
    
    // Resolve based on feature flags
    ast::TypePtr resolved = type;
    
    if (isFeatureEnabled("null_safety")) {
        resolved = nullSafetyChecker_->resolveType(resolved);
    }
    
    if (isFeatureEnabled("result_option")) {
        resolved = resultOptionChecker_->resolveType(resolved);
    }
    
    // Cache the result
    typeCache_[type->toString()] = resolved;
    return resolved;
}

bool FeatureManager::isTypeCompatible(ast::TypePtr from, ast::TypePtr to) {
    if (!from || !to) return false;
    
    // Basic type compatibility
    if (from->toString() == to->toString()) return true;
    
    // Check null safety compatibility
    if (isFeatureEnabled("null_safety")) {
        if (nullSafetyChecker_->isTypeCompatible(from, to)) {
            return true;
        }
    }
    
    // Extend for Result/Option as needed
    
    return false;
}

bool FeatureManager::canImplicitlyConvert(ast::TypePtr from, ast::TypePtr to) {
    if (!from || !to) return false;
    
    // Check null safety conversions
    if (isFeatureEnabled("null_safety")) {
        if (nullSafetyChecker_->canImplicitlyConvert(from, to)) {
            return true;
        }
    }
    
    // Extend for Result/Option as needed
    
    return false;
}

bool FeatureManager::isErrorType(ast::TypePtr type) {
    if (!type) return false;
    
    TOCIN_UNUSED(type);
    return false;
}

bool FeatureManager::isOptionType(ast::TypePtr type) {
    if (!type) return false;
    
    return type_checker::OptionType::isOptionType(type);
}

bool FeatureManager::isResultType(ast::TypePtr type) {
    if (!type) return false;
    
    return type_checker::ResultType::isResultType(type);
}

bool FeatureManager::isNullableType(ast::TypePtr type) {
    if (!type) return false;
    
    if (isFeatureEnabled("null_safety")) {
        return nullSafetyChecker_->isNullableType(type);
    }
    
    return false;
}

bool FeatureManager::isNonNullType(ast::TypePtr type) {
    if (!type) return false;
    
    if (isFeatureEnabled("null_safety")) {
        return nullSafetyChecker_->isNonNullType(type);
    }
    
    return false;
}

bool FeatureManager::canMove(ast::ExprPtr expr) {
    if (!expr) return false;
    
    if (isFeatureEnabled("move_semantics")) {
        return moveSemanticsChecker_->canMove(expr);
    }
    
    return false;
}

bool FeatureManager::shouldMove(ast::ExprPtr expr) {
    if (!expr) return false;
    
    if (isFeatureEnabled("move_semantics")) {
        return moveSemanticsChecker_->shouldMove(expr);
    }
    
    return false;
}

bool FeatureManager::isAsyncFunction(ast::FunctionDeclPtr function) {
    if (!function) return false;
    
    // Check if function is marked as async
    // This would typically check function attributes or return type
    return false; // Placeholder
}

bool FeatureManager::isLinqExpression(ast::ExprPtr expr) {
    if (!expr) return false;
    
    // Check if expression is a LINQ-style operation
    // This would check for specific method calls like where, select, etc.
    return false; // Placeholder
}

bool FeatureManager::isFeatureEnabled(const std::string& featureName) const {
    auto it = featureFlags_.find(featureName);
    return it != featureFlags_.end() && it->second;
}

void FeatureManager::enableFeature(const std::string& featureName) {
    featureFlags_[featureName] = true;
}

void FeatureManager::disableFeature(const std::string& featureName) {
    featureFlags_[featureName] = false;
}

void FeatureManager::reportFeatureError(const std::string& message, ast::ASTNode* node) {
    errorHandler_.reportError(error::ErrorCode::T001_TYPE_MISMATCH, message);
}

std::vector<std::string> FeatureManager::getActiveFeatures() const {
    std::vector<std::string> active;
    for (const auto& pair : featureFlags_) {
        if (pair.second) {
            active.push_back(pair.first);
        }
    }
    return active;
}

// FeatureIntegrationUtils implementation
bool FeatureIntegrationUtils::isAdvancedType(ast::TypePtr type) {
    if (!type) return false;
    
    // Check for advanced type patterns
    std::string typeStr = type->toString();
    return typeStr.find("Option<") != std::string::npos ||
           typeStr.find("Result<") != std::string::npos ||
           typeStr.find("?") != std::string::npos ||
           typeStr.find("&") != std::string::npos ||
           typeStr.find("mut ") != std::string::npos;
}

std::string FeatureIntegrationUtils::getAdvancedTypeDescription(ast::TypePtr type) {
    if (!type) return "unknown";
    
    std::string typeStr = type->toString();
    if (typeStr.find("Option<") != std::string::npos) {
        return "optional type";
    } else if (typeStr.find("Result<") != std::string::npos) {
        return "result type";
    } else if (typeStr.find("?") != std::string::npos) {
        return "nullable type";
    } else if (typeStr.find("&") != std::string::npos) {
        return "reference type";
    } else if (typeStr.find("mut ") != std::string::npos) {
        return "mutable reference type";
    }
    
    return "basic type";
}

bool FeatureIntegrationUtils::requiresSpecialHandling(ast::ExprPtr expr) {
    if (!expr) return false;
    
    // Check for expressions that require special feature handling
    // This would check for ownership transfers, null checks, etc.
    return false; // Placeholder
}

std::vector<std::string> FeatureIntegrationUtils::getRequiredFeatures(ast::ExprPtr expr) {
    std::vector<std::string> features;
    if (!expr) return features;
    
    // Analyze expression to determine required features
    // This would check for ownership, null safety, etc.
    return features; // Placeholder
}

bool FeatureIntegrationUtils::areTypesCompatible(ast::TypePtr type1, ast::TypePtr type2, FeatureManager& manager) {
    return manager.isTypeCompatible(type1, type2);
}

int FeatureIntegrationUtils::getComplexityScore(ast::ExprPtr expr) {
    if (!expr) return 0;
    
    // Calculate complexity based on advanced features used
    int score = 1;
    
    // Add complexity for advanced features
    // This would check for ownership, null safety, etc.
    
    return score;
}

} // namespace type_checker