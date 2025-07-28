#ifndef NULL_SAFETY_H
#define NULL_SAFETY_H

#include "../ast/types.h"
#include "../ast/ast.h"
#include "../error/error_handler.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace type_checker {

/**
 * @brief Null safety checker for Kotlin-like null safety
 */
class NullSafetyChecker {
public:
    NullSafetyChecker(error::ErrorHandler& errorHandler);
    ~NullSafetyChecker();

    // Expression checking
    bool checkExpression(ast::ExprPtr expr);
    bool checkStatement(ast::StmtPtr stmt);
    bool checkFunction(ast::FunctionDeclPtr function);

    // Null safety analysis
    bool isNullableType(ast::TypePtr type);
    bool isNonNullType(ast::TypePtr type);
    bool canBeNull(ast::ExprPtr expr);
    bool isNullCheck(ast::ExprPtr expr);
    bool isSafeCall(ast::ExprPtr expr);
    bool isElvisOperator(ast::ExprPtr expr);

    // Type operations
    ast::TypePtr makeNullable(ast::TypePtr type);
    ast::TypePtr makeNonNull(ast::TypePtr type);
    ast::TypePtr resolveType(ast::TypePtr type);

    // Null safety operators
    bool checkSafeCall(ast::ExprPtr expr);
    bool checkElvisOperator(ast::ExprPtr expr);
    bool checkNullAssertion(ast::ExprPtr expr);
    bool checkNullCheck(ast::ExprPtr expr);

    // Flow analysis
    bool analyzeNullFlow(ast::StmtPtr stmt);
    bool isNullGuarded(ast::ExprPtr expr);
    bool isDefinitelyNull(ast::ExprPtr expr);
    bool isDefinitelyNonNull(ast::ExprPtr expr);

    // Error reporting
    void reportNullSafetyError(const std::string& message, ast::Node* node = nullptr);

private:
    error::ErrorHandler& errorHandler_;
    
    // Null safety state tracking
    std::unordered_map<std::string, bool> nullableVariables_;
    std::unordered_map<std::string, bool> nullCheckedVariables_;
    std::unordered_map<std::string, bool> safeCallVariables_;
    
    // Flow analysis state
    std::unordered_set<std::string> definitelyNull_;
    std::unordered_set<std::string> definitelyNonNull_;
    std::unordered_set<std::string> nullGuarded_;
    
    // Helper methods
    bool checkVariableNullSafety(const std::string& variableName);
    bool checkExpressionNullSafety(ast::ExprPtr expr);
    bool checkStatementNullSafety(ast::StmtPtr stmt);
    
    std::string getVariableName(ast::ExprPtr expr);
    bool isVariableExpression(ast::ExprPtr expr);
    bool isNullLiteral(ast::ExprPtr expr);
    bool isNotNullLiteral(ast::ExprPtr expr);
    
    void markAsNullChecked(const std::string& variableName);
    void markAsSafeCall(const std::string& variableName);
    void markAsDefinitelyNull(const std::string& variableName);
    void markAsDefinitelyNonNull(const std::string& variableName);
    void markAsNullGuarded(const std::string& variableName);
};

/**
 * @brief Null safety utilities
 */
class NullSafetyUtils {
public:
    // Type analysis
    static bool isNullableType(ast::TypePtr type);
    static bool isNonNullType(ast::TypePtr type);
    static ast::TypePtr makeNullable(ast::TypePtr type);
    static ast::TypePtr makeNonNull(ast::TypePtr type);
    static ast::TypePtr extractInnerType(ast::TypePtr nullableType);
    
    // Expression analysis
    static bool isNullLiteral(ast::ExprPtr expr);
    static bool isNotNullLiteral(ast::ExprPtr expr);
    static bool isNullCheck(ast::ExprPtr expr);
    static bool isSafeCall(ast::ExprPtr expr);
    static bool isElvisOperator(ast::ExprPtr expr);
    static bool isNullAssertion(ast::ExprPtr expr);
    
    // Operator utilities
    static std::string getSafeCallOperator();
    static std::string getElvisOperator();
    static std::string getNullAssertionOperator();
    static std::string getNullCheckOperator();
    
    // Error message utilities
    static std::string formatNullPointerError(const std::string& variableName);
    static std::string formatNullableAssignmentError(const std::string& variableName);
    static std::string formatNullCheckError(const std::string& variableName);
    static std::string formatSafeCallError(const std::string& variableName);
    
    // Flow analysis utilities
    static bool canBeNull(ast::ExprPtr expr);
    static bool isDefinitelyNull(ast::ExprPtr expr);
    static bool isDefinitelyNonNull(ast::ExprPtr expr);
    static bool isNullGuarded(ast::ExprPtr expr);
    
private:
    NullSafetyUtils() = delete;
};

/**
 * @brief Null safety flow analyzer
 */
class NullSafetyFlowAnalyzer {
public:
    struct FlowState {
        bool isNullGuarded;
        bool isDefinitelyNull;
        bool isDefinitelyNonNull;
        bool isNullable;
        
        FlowState() : isNullGuarded(false), isDefinitelyNull(false), 
                     isDefinitelyNonNull(false), isNullable(true) {}
    };

    NullSafetyFlowAnalyzer();
    ~NullSafetyFlowAnalyzer();

    // Flow analysis
    bool analyzeFlow(ast::StmtPtr stmt);
    bool analyzeExpressionFlow(ast::ExprPtr expr);
    bool analyzeConditionalFlow(ast::ExprPtr condition, ast::StmtPtr thenStmt, ast::StmtPtr elseStmt);
    
    // State management
    void enterScope();
    void exitScope();
    void addVariable(const std::string& name);
    void removeVariable(const std::string& name);
    
    // State queries
    bool isVariableNullGuarded(const std::string& name) const;
    bool isVariableDefinitelyNull(const std::string& name) const;
    bool isVariableDefinitelyNonNull(const std::string& name) const;
    bool isVariableNullable(const std::string& name) const;
    
    // State updates
    void markAsNullGuarded(const std::string& name);
    void markAsDefinitelyNull(const std::string& name);
    void markAsDefinitelyNonNull(const std::string& name);
    void markAsNullable(const std::string& name);
    
    // Cleanup
    void clear();

private:
    std::unordered_map<std::string, FlowState> flowStates_;
    std::vector<std::unordered_set<std::string>> scopeVariables_;
    size_t currentScopeLevel_;
};

} // namespace type_checker

#endif // NULL_SAFETY_H
