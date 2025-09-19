#ifndef OWNERSHIP_H
#define OWNERSHIP_H

#include "../ast/types.h"
#include "../ast/ast.h"
#include "../error/error_handler.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace type_checker {

// Ownership state enums
enum class BorrowState {
    NOT_BORROWED,
    IMMUTABLE_BORROWED,
    MUTABLE_BORROWED
};

enum class MoveState {
    NOT_MOVED,
    MOVED
};

/**
 * @brief Ownership checker for Rust-like ownership semantics
 */
class OwnershipChecker {
public:
    OwnershipChecker(error::ErrorHandler& errorHandler);
    ~OwnershipChecker();

    // Expression checking
    bool checkExpression(ast::ExprPtr expr);
    bool checkStatement(ast::StmtPtr stmt);
    bool checkFunction(ast::FunctionDeclPtr function);

    // Ownership analysis
    bool canMove(ast::ExprPtr expr);
    bool shouldMove(ast::ExprPtr expr);
    bool isBorrowed(ast::ExprPtr expr);
    bool isMutable(ast::ExprPtr expr);

    // Lifetime analysis
    struct Lifetime {
        std::string name;
        size_t scopeLevel;
        bool isStatic;
        std::vector<std::string> dependencies;
    };

    bool analyzeLifetimes(ast::StmtPtr stmt, std::vector<Lifetime>& lifetimes);
    bool checkLifetimeValidity(const std::string& lifetime, const std::vector<Lifetime>& lifetimes);

    // Borrow checking
    bool checkBorrowRules(ast::ExprPtr expr);
    bool hasConflictingBorrows(ast::ExprPtr expr);
    bool isBorrowValid(ast::ExprPtr borrowed, ast::ExprPtr borrower);

    // Move semantics
    bool checkMoveValidity(ast::ExprPtr from, ast::ExprPtr to);
    bool isMoveSafe(ast::ExprPtr expr);
    void markAsMoved(ast::ExprPtr expr);
    void markAsBorrowed(ast::ExprPtr expr, bool isMutable);

    // Ownership transfer
    bool canTransferOwnership(ast::ExprPtr from, ast::ExprPtr to);
    bool transferOwnership(ast::ExprPtr from, ast::ExprPtr to);

    // Error reporting
    void reportOwnershipError(const std::string& message, ast::Node* node = nullptr);

private:
    error::ErrorHandler& errorHandler_;
    
    // Ownership state tracking
    std::unordered_map<std::string, bool> movedVariables_;
    std::unordered_map<std::string, bool> borrowedVariables_;
    std::unordered_map<std::string, bool> mutableBorrows_;
    std::unordered_map<std::string, size_t> borrowCounts_;
    
    // Scope management
    size_t currentScopeLevel_;
    std::vector<std::unordered_set<std::string>> scopeVariables_;
    
    // Helper methods
    void enterScope();
    void exitScope();
    void addVariableToScope(const std::string& name);
    bool isVariableInScope(const std::string& name);
    void clearScopeVariables();
    
    bool checkVariableOwnership(const std::string& variableName);
    bool checkExpressionOwnership(ast::ExprPtr expr);
    bool checkStatementOwnership(ast::StmtPtr stmt);
    
    std::string getVariableName(ast::ExprPtr expr);
    bool isVariableExpression(ast::ExprPtr expr);
    bool isAssignmentExpression(ast::ExprPtr expr);
    bool isFunctionCall(ast::ExprPtr expr);
};

/**
 * @brief Ownership utilities
 */
class OwnershipUtils {
public:
    // Variable analysis
    static bool isOwnedVariable(const std::string& variableName);
    static bool isBorrowedVariable(const std::string& variableName);
    static bool isMutableVariable(const std::string& variableName);
    
    // Expression analysis
    static bool isMoveExpression(ast::ExprPtr expr);
    static bool isBorrowExpression(ast::ExprPtr expr);
    static bool isMutableBorrowExpression(ast::ExprPtr expr);
    static bool isImmutableBorrowExpression(ast::ExprPtr expr);
    
    // Type analysis
    static bool isOwnedType(ast::TypePtr type);
    static bool isBorrowedType(ast::TypePtr type);
    static bool isMutableType(ast::TypePtr type);
    static ast::TypePtr makeOwnedType(ast::TypePtr type);
    static ast::TypePtr makeBorrowedType(ast::TypePtr type, bool isMutable);
    
    // Lifetime utilities
    static std::string generateLifetimeName();
    static bool isValidLifetimeName(const std::string& name);
    static std::vector<std::string> extractLifetimes(ast::TypePtr type);
    
    // Borrow checking utilities
    static bool canBorrowImmutably(const std::string& variableName);
    static bool canBorrowMutably(const std::string& variableName);
    static bool hasActiveBorrows(const std::string& variableName);
    static bool hasMutableBorrow(const std::string& variableName);
    
    // Move checking utilities
    static bool canMoveVariable(const std::string& variableName);
    static bool isVariableMoved(const std::string& variableName);
    static bool isVariableBorrowed(const std::string& variableName);
    
    // Error message utilities
    static std::string formatMoveError(const std::string& variableName);
    static std::string formatBorrowError(const std::string& variableName, bool isMutable);
    static std::string formatLifetimeError(const std::string& lifetime);
    
private:
    OwnershipUtils() = delete;
};

/**
 * @brief Ownership state tracker
 */
class OwnershipStateTracker {
public:
    struct VariableState {
        bool isMoved;
        bool isBorrowed;
        bool isMutableBorrow;
        size_t borrowCount;
        size_t scopeLevel;
        std::vector<std::string> activeLifetimes;
        
        VariableState() : isMoved(false), isBorrowed(false), isMutableBorrow(false), 
                         borrowCount(0), scopeLevel(0) {}
    };

    OwnershipStateTracker();
    ~OwnershipStateTracker();

    // State management
    void enterScope();
    void exitScope();
    void addVariable(const std::string& name);
    void removeVariable(const std::string& name);
    
    // Variable state tracking
    void markAsMoved(const std::string& name);
    void markAsBorrowed(const std::string& name, bool isMutable);
    void markAsUnborrowed(const std::string& name);
    
    // State queries
    bool isVariableMoved(const std::string& name) const;
    bool isVariableBorrowed(const std::string& name) const;
    bool isVariableMutableBorrowed(const std::string& name) const;
    bool canMoveVariable(const std::string& name) const;
    bool canBorrowVariable(const std::string& name, bool isMutable) const;
    
    // Helper methods for OwnershipUtils
    BorrowState getBorrowState(const std::string& variableName) const;
    MoveState getMoveState(const std::string& variableName) const;
    
    // Lifetime tracking
    void addLifetime(const std::string& variable, const std::string& lifetime);
    void removeLifetime(const std::string& variable, const std::string& lifetime);
    std::vector<std::string> getVariableLifetimes(const std::string& variable) const;
    
    // State inspection
    std::vector<std::string> getMovedVariables() const;
    std::vector<std::string> getBorrowedVariables() const;
    std::vector<std::string> getMutableBorrowedVariables() const;
    size_t getBorrowCount(const std::string& variable) const;
    
    // Cleanup
    void clear();

private:
    std::unordered_map<std::string, VariableState> variableStates_;
    size_t currentScopeLevel_;
    std::vector<std::unordered_set<std::string>> scopeVariables_;
};

// Declare the global tracker so helper functions can link
extern OwnershipStateTracker globalOwnershipTracker;

} // namespace type_checker

#endif // OWNERSHIP_H
