#pragma once

#include "../ast/types.h"
#include "../ast/ast.h"
#include "../error/error_handler.h"
#include "ownership.h"
#include "result_option.h"
#include "null_safety.h"
#include "extension_functions.h"
#include "move_semantics.h"
#include "traits.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace type_checker {

/**
 * @brief Central manager for all advanced language features
 */
class FeatureManager {
public:
    FeatureManager(error::ErrorHandler& errorHandler);
    ~FeatureManager();

    // Initialization
    bool initialize();
    void finalize();

    // Feature component access
    OwnershipChecker& getOwnershipChecker() { return *ownershipChecker_; }
    const OwnershipChecker& getOwnershipChecker() const { return *ownershipChecker_; }

    ResultOptionMatcher& getResultOptionChecker() { return *resultOptionChecker_; }
    const ResultOptionMatcher& getResultOptionChecker() const { return *resultOptionChecker_; }

    NullSafetyChecker& getNullSafetyChecker() { return *nullSafetyChecker_; }
    const NullSafetyChecker& getNullSafetyChecker() const { return *nullSafetyChecker_; }

    ExtensionManager& getExtensionFunctionChecker() { return *extensionFunctionChecker_; }
    const ExtensionManager& getExtensionFunctionChecker() const { return *extensionFunctionChecker_; }

    MoveChecker& getMoveSemanticsChecker() { return *moveSemanticsChecker_; }
    const MoveChecker& getMoveSemanticsChecker() const { return *moveSemanticsChecker_; }

    type_checker::TraitChecker& getTraitChecker() { return *traitChecker_; }
    const type_checker::TraitChecker& getTraitChecker() const { return *traitChecker_; }

    // Feature integration methods
    bool checkExpression(ast::ExprPtr expr, ast::TypePtr expectedType = nullptr);
    bool checkStatement(ast::StmtPtr stmt);
    bool checkFunction(ast::FunctionDeclPtr function);
    bool checkClass(ast::ClassDeclPtr classDecl);
    bool checkTrait(ast::TraitDeclPtr traitDecl);

    // Type system integration
    ast::TypePtr resolveType(ast::TypePtr type);
    bool isTypeCompatible(ast::TypePtr from, ast::TypePtr to);
    ast::TypePtr getCommonType(ast::TypePtr type1, ast::TypePtr type2);

    // Advanced type operations
    bool canImplicitlyConvert(ast::TypePtr from, ast::TypePtr to);
    bool canExplicitlyConvert(ast::TypePtr from, ast::TypePtr to);
    ast::TypePtr performTypeConversion(ast::TypePtr from, ast::TypePtr to, ast::ExprPtr expr);

    // Generic type support
    struct GenericContext {
        std::unordered_map<std::string, ast::TypePtr> typeBindings;
        std::vector<std::string> constraints;
    };

    bool instantiateGenericType(ast::TypePtr genericType, const GenericContext& context, ast::TypePtr& result);
    bool checkGenericConstraints(const GenericContext& context);

    // Trait implementation checking
    bool checkTraitImplementation(ast::TypePtr type, ast::TypePtr traitType);
    std::vector<std::string> getMissingTraitMethods(ast::TypePtr type, ast::TypePtr traitType);

    // Ownership and lifetime analysis
    struct LifetimeInfo {
        std::string name;
        size_t scopeLevel;
        bool isStatic;
        std::vector<std::string> dependencies;
    };

    bool analyzeLifetimes(ast::StmtPtr stmt, std::vector<LifetimeInfo>& lifetimes);
    bool checkOwnershipTransfer(ast::ExprPtr from, ast::ExprPtr to);

    // Error handling integration
    bool isErrorType(ast::TypePtr type);
    bool isOptionType(ast::TypePtr type);
    bool isResultType(ast::TypePtr type);
    ast::TypePtr extractInnerType(ast::TypePtr wrapperType);

    // Null safety integration
    bool isNullableType(ast::TypePtr type);
    bool isNonNullType(ast::TypePtr type);
    ast::TypePtr makeNullable(ast::TypePtr type);
    ast::TypePtr makeNonNull(ast::TypePtr type);

    // Extension function resolution
    struct ExtensionCandidate {
        ast::FunctionDeclPtr function;
        ast::TypePtr receiverType;
        int priority;
        bool isExactMatch;
    };

    std::vector<ExtensionCandidate> findExtensionFunctions(ast::TypePtr receiverType, const std::string& methodName);
    ast::FunctionDeclPtr resolveExtensionCall(ast::CallExpr* call);

    // Move semantics integration
    bool canMove(ast::ExprPtr expr);
    bool shouldMove(ast::ExprPtr expr);
    ast::ExprPtr insertMoveOperation(ast::ExprPtr expr);

    // Pattern matching support
    struct PatternMatchInfo {
        ast::ExprPtr pattern;
        ast::TypePtr patternType;
        std::unordered_map<std::string, ast::TypePtr> bindings;
        bool isExhaustive;
    };

    bool checkPatternMatch(ast::ExprPtr value, const std::vector<PatternMatchInfo>& patterns);
    bool isPatternExhaustive(ast::TypePtr type, const std::vector<PatternMatchInfo>& patterns);

    // Async/await support
    bool isAsyncFunction(ast::FunctionDeclPtr function);
    bool isAwaitExpression(ast::ExprPtr expr);
    ast::TypePtr getAsyncReturnType(ast::TypePtr functionType);

    // LINQ integration
    bool isLinqExpression(ast::ExprPtr expr);
    ast::TypePtr inferLinqType(ast::ExprPtr expr);

    // Feature flags
    bool isFeatureEnabled(const std::string& featureName) const;
    void enableFeature(const std::string& featureName);
    void disableFeature(const std::string& featureName);

    // Diagnostics and debugging
    void dumpFeatureState() const;
    std::vector<std::string> getActiveFeatures() const;
    std::string getFeatureStatistics() const;

private:
    error::ErrorHandler& errorHandler_;
    
    // Feature components
    std::unique_ptr<OwnershipChecker> ownershipChecker_;
    std::unique_ptr<ResultOptionMatcher> resultOptionChecker_;
    std::unique_ptr<NullSafetyChecker> nullSafetyChecker_;
    std::unique_ptr<ExtensionManager> extensionFunctionChecker_;
    std::unique_ptr<MoveChecker> moveSemanticsChecker_;
    std::unique_ptr<type_checker::TraitChecker> traitChecker_;

    // Feature flags
    std::unordered_map<std::string, bool> featureFlags_;

    // Integration state
    bool initialized_;
    std::unordered_map<std::string, ast::TypePtr> typeCache_;
    std::vector<GenericContext> genericContextStack_;

    // Helper methods
    void initializeFeatureFlags();
    bool validateFeatureInteraction(const std::string& feature1, const std::string& feature2);
    void reportFeatureError(const std::string& message, ast::Node* node = nullptr);
};

/**
 * @brief Feature integration utilities
 */
class FeatureIntegrationUtils {
public:
    // Type system utilities
    static bool isAdvancedType(ast::TypePtr type);
    static std::string getAdvancedTypeDescription(ast::TypePtr type);
    static ast::TypePtr simplifyAdvancedType(ast::TypePtr type);

    // Expression utilities
    static bool requiresSpecialHandling(ast::ExprPtr expr);
    static std::vector<std::string> getRequiredFeatures(ast::ExprPtr expr);
    static bool canOptimizeExpression(ast::ExprPtr expr);

    // Statement utilities
    static bool isAdvancedStatement(ast::StmtPtr stmt);
    static std::vector<std::string> getStatementDependencies(ast::StmtPtr stmt);

    // Compatibility checking
    static bool areTypesCompatible(ast::TypePtr type1, ast::TypePtr type2, FeatureManager& manager);
    static bool canCoexist(const std::string& feature1, const std::string& feature2);

    // Performance analysis
    static int getComplexityScore(ast::ExprPtr expr);
    static std::vector<std::string> getOptimizationOpportunities(ast::StmtPtr stmt);

private:
    FeatureIntegrationUtils() = delete;
};

} // namespace type_checker
