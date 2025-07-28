#include "ownership.h"
#include <iostream>
#include <sstream>
#include <algorithm>

namespace type_checker {

OwnershipChecker::OwnershipChecker(error::ErrorHandler& errorHandler)
    : errorHandler_(errorHandler), currentScopeLevel_(0) {
    enterScope(); // Start with global scope
}

OwnershipChecker::~OwnershipChecker() {
    // Clean up scopes
    while (currentScopeLevel_ > 0) {
        exitScope();
    }
}

bool OwnershipChecker::checkExpression(ast::ExprPtr expr) {
    if (!expr) return true;
    
    try {
        return checkExpressionOwnership(expr);
    } catch (const std::exception& e) {
        reportOwnershipError("Expression ownership check failed: " + std::string(e.what()));
        return false;
    }
}

bool OwnershipChecker::checkStatement(ast::StmtPtr stmt) {
    if (!stmt) return true;
    
    try {
        return checkStatementOwnership(stmt);
    } catch (const std::exception& e) {
        reportOwnershipError("Statement ownership check failed: " + std::string(e.what()));
        return false;
    }
}

bool OwnershipChecker::checkFunction(ast::FunctionDeclPtr function) {
    if (!function) return true;
    
    try {
        enterScope(); // Function scope
        
        // Check function parameters
        for (const auto& param : function->getParameters()) {
            addVariableToScope(param.name);
        }
        
        // Check function body
        bool result = checkStatementOwnership(function->getBody());
        
        exitScope(); // Exit function scope
        return result;
    } catch (const std::exception& e) {
        reportOwnershipError("Function ownership check failed: " + std::string(e.what()));
        return false;
    }
}

bool OwnershipChecker::canMove(ast::ExprPtr expr) {
    if (!expr) return false;
    
    std::string varName = getVariableName(expr);
    if (varName.empty()) return true; // Not a variable expression
    
    // Check if variable is moved
    if (movedVariables_.find(varName) != movedVariables_.end()) {
        return false;
    }
    
    // Check if variable is borrowed
    if (borrowedVariables_.find(varName) != borrowedVariables_.end()) {
        return false;
    }
    
    return true;
}

bool OwnershipChecker::shouldMove(ast::ExprPtr expr) {
    if (!expr) return false;
    
    // Check if this is a move expression (e.g., assignment, function call)
    return isAssignmentExpression(expr) || isFunctionCall(expr);
}

bool OwnershipChecker::isBorrowed(ast::ExprPtr expr) {
    if (!expr) return false;
    
    std::string varName = getVariableName(expr);
    if (varName.empty()) return false;
    
    return borrowedVariables_.find(varName) != borrowedVariables_.end();
}

bool OwnershipChecker::isMutable(ast::ExprPtr expr) {
    if (!expr) return false;
    
    std::string varName = getVariableName(expr);
    if (varName.empty()) return false;
    
    auto it = mutableBorrows_.find(varName);
    return it != mutableBorrows_.end() && it->second;
}

bool OwnershipChecker::analyzeLifetimes(ast::StmtPtr stmt, std::vector<Lifetime>& lifetimes) {
    if (!stmt) return true;
    
    try {
        // This is a simplified lifetime analysis
        // In a real implementation, you would analyze the AST to determine lifetimes
        
        // For now, create a basic lifetime for the statement
        Lifetime lifetime;
        lifetime.name = "lifetime_" + std::to_string(lifetimes.size());
        lifetime.scopeLevel = currentScopeLevel_;
        lifetime.isStatic = false;
        
        lifetimes.push_back(lifetime);
        return true;
    } catch (const std::exception& e) {
        reportOwnershipError("Lifetime analysis failed: " + std::string(e.what()));
        return false;
    }
}

bool OwnershipChecker::checkLifetimeValidity(const std::string& lifetime, const std::vector<Lifetime>& lifetimes) {
    // Check if lifetime is valid
    for (const auto& lt : lifetimes) {
        if (lt.name == lifetime) {
            return true;
        }
    }
    return false;
}

bool OwnershipChecker::checkBorrowRules(ast::ExprPtr expr) {
    if (!expr) return true;
    
    // Check for conflicting borrows
    if (hasConflictingBorrows(expr)) {
        reportOwnershipError("Conflicting borrows detected");
        return false;
    }
    
    return true;
}

bool OwnershipChecker::hasConflictingBorrows(ast::ExprPtr expr) {
    if (!expr) return false;
    
    // Check if there are both mutable and immutable borrows
    std::string varName = getVariableName(expr);
    if (varName.empty()) return false;
    
    bool hasMutable = mutableBorrows_.find(varName) != mutableBorrows_.end();
    bool hasImmutable = borrowedVariables_.find(varName) != borrowedVariables_.end();
    
    return hasMutable && hasImmutable;
}

bool OwnershipChecker::isBorrowValid(ast::ExprPtr borrowed, ast::ExprPtr borrower) {
    if (!borrowed || !borrower) return false;
    
    std::string borrowedVar = getVariableName(borrowed);
    std::string borrowerVar = getVariableName(borrower);
    
    if (borrowedVar.empty() || borrowerVar.empty()) return true;
    
    // Check if borrowed variable is still valid
    if (movedVariables_.find(borrowedVar) != movedVariables_.end()) {
        return false;
    }
    
    return true;
}

bool OwnershipChecker::checkMoveValidity(ast::ExprPtr from, ast::ExprPtr to) {
    if (!from || !to) return false;
    
    // Check if source can be moved
    if (!canMove(from)) {
        return false;
    }
    
    // Check if destination can receive the move
    std::string toVar = getVariableName(to);
    if (!toVar.empty()) {
        if (borrowedVariables_.find(toVar) != borrowedVariables_.end()) {
            return false;
        }
    }
    
    return true;
}

bool OwnershipChecker::isMoveSafe(ast::ExprPtr expr) {
    if (!expr) return false;
    
    std::string varName = getVariableName(expr);
    if (varName.empty()) return true;
    
    // Check if variable is safe to move
    return !isBorrowed(expr) && !isVariableExpression(expr);
}

void OwnershipChecker::markAsMoved(ast::ExprPtr expr) {
    if (!expr) return;
    
    std::string varName = getVariableName(expr);
    if (!varName.empty()) {
        movedVariables_[varName] = true;
    }
}

void OwnershipChecker::markAsBorrowed(ast::ExprPtr expr, bool mutable) {
    if (!expr) return;
    
    std::string varName = getVariableName(expr);
    if (!varName.empty()) {
        borrowedVariables_[varName] = true;
        if (mutable) {
            mutableBorrows_[varName] = true;
        }
        borrowCounts_[varName]++;
    }
}

bool OwnershipChecker::canTransferOwnership(ast::ExprPtr from, ast::ExprPtr to) {
    return checkMoveValidity(from, to);
}

bool OwnershipChecker::transferOwnership(ast::ExprPtr from, ast::ExprPtr to) {
    if (!canTransferOwnership(from, to)) {
        return false;
    }
    
    markAsMoved(from);
    return true;
}

void OwnershipChecker::reportOwnershipError(const std::string& message, ast::ASTNodePtr node) {
    errorHandler_.reportError(error::ErrorCode::B009_INVALID_OWNERSHIP, message);
}

void OwnershipChecker::enterScope() {
    currentScopeLevel_++;
    scopeVariables_.push_back(std::unordered_set<std::string>());
}

void OwnershipChecker::exitScope() {
    if (currentScopeLevel_ > 0) {
        // Remove variables from current scope
        if (!scopeVariables_.empty()) {
            for (const auto& varName : scopeVariables_.back()) {
                movedVariables_.erase(varName);
                borrowedVariables_.erase(varName);
                mutableBorrows_.erase(varName);
                borrowCounts_.erase(varName);
            }
            scopeVariables_.pop_back();
        }
        currentScopeLevel_--;
    }
}

void OwnershipChecker::addVariableToScope(const std::string& name) {
    if (currentScopeLevel_ > 0 && !scopeVariables_.empty()) {
        scopeVariables_.back().insert(name);
    }
}

bool OwnershipChecker::isVariableInScope(const std::string& name) {
    for (const auto& scope : scopeVariables_) {
        if (scope.find(name) != scope.end()) {
            return true;
        }
    }
    return false;
}

void OwnershipChecker::clearScopeVariables() {
    scopeVariables_.clear();
    movedVariables_.clear();
    borrowedVariables_.clear();
    mutableBorrows_.clear();
    borrowCounts_.clear();
}

bool OwnershipChecker::checkVariableOwnership(const std::string& variableName) {
    if (variableName.empty()) return true;
    
    // Check if variable is moved
    if (movedVariables_.find(variableName) != movedVariables_.end()) {
        reportOwnershipError("Use of moved variable: " + variableName);
        return false;
    }
    
    return true;
}

bool OwnershipChecker::checkExpressionOwnership(ast::ExprPtr expr) {
    if (!expr) return true;
    
    // Check variable ownership
    std::string varName = getVariableName(expr);
    if (!varName.empty()) {
        if (!checkVariableOwnership(varName)) {
            return false;
        }
    }
    
    // Check borrow rules
    if (!checkBorrowRules(expr)) {
        return false;
    }
    
    return true;
}

bool OwnershipChecker::checkStatementOwnership(ast::StmtPtr stmt) {
    if (!stmt) return true;
    
    // This is a simplified implementation
    // In a real implementation, you would check different statement types
    
    return true;
}

std::string OwnershipChecker::getVariableName(ast::ExprPtr expr) {
    if (!expr) return "";
    
    // Extract variable name from expression
    if (auto varExpr = std::dynamic_pointer_cast<ast::VariableExpr>(expr)) {
        return varExpr->name;
    }
    
    // Check for property access (obj.prop)
    if (auto getExpr = std::dynamic_pointer_cast<ast::GetExpr>(expr)) {
        return getVariableName(getExpr->object);
    }
    
    // Check for array access (arr[index])
    if (auto indexExpr = std::dynamic_pointer_cast<ast::IndexExpr>(expr)) {
        return getVariableName(indexExpr->object);
    }
    
    return "";
}

bool OwnershipChecker::isVariableExpression(ast::ExprPtr expr) {
    if (!expr) return false;
    
    // Check if expression is a variable reference
    if (std::dynamic_pointer_cast<ast::VariableExpr>(expr)) {
        return true;
    }
    
    // Check for property access
    if (std::dynamic_pointer_cast<ast::GetExpr>(expr)) {
        return true;
    }
    
    // Check for array access
    if (std::dynamic_pointer_cast<ast::IndexExpr>(expr)) {
        return true;
    }
    
    return false;
}

bool OwnershipChecker::isAssignmentExpression(ast::ExprPtr expr) {
    if (!expr) return false;
    
    // Check if expression is an assignment
    if (std::dynamic_pointer_cast<ast::AssignExpr>(expr)) {
        return true;
    }
    
    return false;
}

bool OwnershipChecker::isFunctionCall(ast::ExprPtr expr) {
    if (!expr) return false;
    
    // Check if expression is a function call
    if (std::dynamic_pointer_cast<ast::CallExpr>(expr)) {
        return true;
    }
    
    return false;
}

// OwnershipUtils implementation
bool OwnershipUtils::isOwnedVariable(const std::string& variableName) {
    // Check if variable name indicates ownership
    return variableName.find("owned_") == 0 || variableName.find("own_") == 0;
}

bool OwnershipUtils::isBorrowedVariable(const std::string& variableName) {
    // Check if variable name indicates borrowing
    return variableName.find("borrowed_") == 0 || variableName.find("ref_") == 0;
}

bool OwnershipUtils::isMutableVariable(const std::string& variableName) {
    // Check if variable name indicates mutability
    return variableName.find("mut_") == 0 || variableName.find("mutable_") == 0;
}

bool OwnershipUtils::isMoveExpression(ast::ExprPtr expr) {
    if (!expr) return false;
    
    // Check for move expressions (move x)
    if (auto unaryExpr = std::dynamic_pointer_cast<ast::UnaryExpr>(expr)) {
        if (unaryExpr->op.type == lexer::TokenType::MOVE) {
            return true;
        }
    }
    
    return false;
}

bool OwnershipUtils::isBorrowExpression(ast::ExprPtr expr) {
    if (!expr) return false;
    
    // Check for borrow expressions (&x, &mut x)
    if (auto unaryExpr = std::dynamic_pointer_cast<ast::UnaryExpr>(expr)) {
        if (unaryExpr->op.type == lexer::TokenType::BORROW || 
            unaryExpr->op.type == lexer::TokenType::MUTABLE_BORROW) {
            return true;
        }
    }
    
    return false;
}

bool OwnershipUtils::isMutableBorrowExpression(ast::ExprPtr expr) {
    if (!expr) return false;
    
    // Check for mutable borrow expressions (&mut x)
    if (auto unaryExpr = std::dynamic_pointer_cast<ast::UnaryExpr>(expr)) {
        if (unaryExpr->op.type == lexer::TokenType::MUTABLE_BORROW) {
            return true;
        }
    }
    
    return false;
}

bool OwnershipUtils::isImmutableBorrowExpression(ast::ExprPtr expr) {
    if (!expr) return false;
    
    // Check for immutable borrow expressions (&x)
    if (auto unaryExpr = std::dynamic_pointer_cast<ast::UnaryExpr>(expr)) {
        if (unaryExpr->op.type == lexer::TokenType::BORROW) {
            return true;
        }
    }
    
    return false;
}

bool OwnershipUtils::isOwnedType(ast::TypePtr type) {
    if (!type) return false;
    
    std::string typeStr = type->toString();
    
    // Basic types are owned
    if (typeStr == "int" || typeStr == "float" || typeStr == "bool" || 
        typeStr == "string" || typeStr == "char") {
        return true;
    }
    
    // Arrays and owned pointers are owned
    if (typeStr.find("Array<") == 0 || typeStr.find("Box<") == 0) {
        return true;
    }
    
    return false;
}

bool OwnershipUtils::isBorrowedType(ast::TypePtr type) {
    if (!type) return false;
    
    std::string typeStr = type->toString();
    
    // Reference types are borrowed
    if (typeStr.find("&") == 0 || typeStr.find("Ref<") == 0) {
        return true;
    }
    
    return false;
}

bool OwnershipUtils::isMutableType(ast::TypePtr type) {
    if (!type) return false;
    
    std::string typeStr = type->toString();
    
    // Check for mutable indicators
    if (typeStr.find("mut ") == 0 || typeStr.find("&mut ") == 0) {
        return true;
    }
    
    return false;
}

ast::TypePtr OwnershipUtils::makeOwnedType(ast::TypePtr type) {
    if (!type) return nullptr;
    
    std::string typeStr = type->toString();
    
    // Remove reference markers
    if (typeStr.find("&") == 0) {
        typeStr = typeStr.substr(typeStr.find(" ") + 1);
    }
    
    return std::make_shared<ast::SimpleType>(
        lexer::Token(lexer::TokenType::IDENTIFIER, typeStr, "", 0, 0));
}

ast::TypePtr OwnershipUtils::makeBorrowedType(ast::TypePtr type, bool mutable) {
    if (!type) return nullptr;
    
    std::string typeStr = type->toString();
    
    // Add appropriate reference marker
    if (mutable) {
        typeStr = "&mut " + typeStr;
    } else {
        typeStr = "&" + typeStr;
    }
    
    return std::make_shared<ast::SimpleType>(
        lexer::Token(lexer::TokenType::IDENTIFIER, typeStr, "", 0, 0));
}

std::string OwnershipUtils::generateLifetimeName() {
    static int counter = 0;
    return "'a" + std::to_string(counter++);
}

bool OwnershipUtils::isValidLifetimeName(const std::string& name) {
    if (name.empty()) return false;
    
    // Check if it starts with a quote
    if (name[0] != '\'') return false;
    
    // Check if the rest is a valid identifier
    for (size_t i = 1; i < name.length(); ++i) {
        if (!std::isalnum(name[i]) && name[i] != '_') return false;
    }
    
    return true;
}

std::vector<std::string> OwnershipUtils::extractLifetimes(ast::TypePtr type) {
    std::vector<std::string> lifetimes;
    if (!type) return lifetimes;
    
    std::string typeStr = type->toString();
    
    // Simple lifetime extraction
    size_t pos = 0;
    while ((pos = typeStr.find('\'', pos)) != std::string::npos) {
        size_t end = typeStr.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_", pos + 1);
        if (end == std::string::npos) end = typeStr.length();
        
        std::string lifetime = typeStr.substr(pos, end - pos);
        if (isValidLifetimeName(lifetime)) {
            lifetimes.push_back(lifetime);
        }
        
        pos = end;
    }
    
    return lifetimes;
}

bool OwnershipUtils::canBorrowImmutably(const std::string& variableName) {
    // Check if the variable can be borrowed immutably
    return globalOwnershipTracker.canBorrowVariable(variableName, false);
}

bool OwnershipUtils::canBorrowMutably(const std::string& variableName) {
    // Check if the variable can be borrowed mutably
    return globalOwnershipTracker.canBorrowVariable(variableName, true);
}

bool OwnershipUtils::hasActiveBorrows(const std::string& variableName) {
    // Check if the variable has active borrows
    return globalOwnershipTracker.isVariableBorrowed(variableName);
}

bool OwnershipUtils::hasMutableBorrow(const std::string& variableName) {
    // Check if the variable has a mutable borrow
    return globalOwnershipTracker.isVariableMutableBorrowed(variableName);
}

bool OwnershipUtils::canMoveVariable(const std::string& variableName) {
    // Check if the variable can be moved
    return globalOwnershipTracker.canMoveVariable(variableName);
}

bool OwnershipUtils::isVariableMoved(const std::string& variableName) {
    // Check if the variable has been moved
    return globalOwnershipTracker.isVariableMoved(variableName);
}

bool OwnershipUtils::isVariableBorrowed(const std::string& variableName) {
    // Check if the variable is borrowed
    return globalOwnershipTracker.isVariableBorrowed(variableName);
}

std::string OwnershipUtils::formatMoveError(const std::string& variableName) {
    return "Cannot move variable '" + variableName + "' - it has been moved already";
}

std::string OwnershipUtils::formatBorrowError(const std::string& variableName, bool mutable) {
    std::string borrowType = mutable ? "mutable" : "immutable";
    return "Cannot borrow '" + variableName + "' as " + borrowType + " - conflicting borrows";
}

std::string OwnershipUtils::formatLifetimeError(const std::string& lifetime) {
    return "Lifetime '" + lifetime + "' does not live long enough";
}

// OwnershipStateTracker implementation
OwnershipStateTracker::OwnershipStateTracker() : currentScopeLevel_(0) {
    enterScope(); // Start with global scope
}

OwnershipStateTracker::~OwnershipStateTracker() {
    // Clean up scopes
    while (currentScopeLevel_ > 0) {
        exitScope();
    }
}

void OwnershipStateTracker::enterScope() {
    currentScopeLevel_++;
    scopeVariables_.push_back(std::unordered_set<std::string>());
}

void OwnershipStateTracker::exitScope() {
    if (currentScopeLevel_ > 0) {
        // Remove variables from current scope
        if (!scopeVariables_.empty()) {
            for (const auto& varName : scopeVariables_.back()) {
                variableStates_.erase(varName);
            }
            scopeVariables_.pop_back();
        }
        currentScopeLevel_--;
    }
}

void OwnershipStateTracker::addVariable(const std::string& name) {
    if (currentScopeLevel_ > 0 && !scopeVariables_.empty()) {
        scopeVariables_.back().insert(name);
        variableStates_[name] = VariableState();
        variableStates_[name].scopeLevel = currentScopeLevel_;
    }
}

void OwnershipStateTracker::removeVariable(const std::string& name) {
    variableStates_.erase(name);
}

void OwnershipStateTracker::markAsMoved(const std::string& name) {
    auto it = variableStates_.find(name);
    if (it != variableStates_.end()) {
        it->second.isMoved = true;
    }
}

void OwnershipStateTracker::markAsBorrowed(const std::string& name, bool mutable) {
    auto it = variableStates_.find(name);
    if (it != variableStates_.end()) {
        it->second.isBorrowed = true;
        it->second.isMutableBorrow = mutable;
        it->second.borrowCount++;
    }
}

void OwnershipStateTracker::markAsUnborrowed(const std::string& name) {
    auto it = variableStates_.find(name);
    if (it != variableStates_.end()) {
        it->second.isBorrowed = false;
        it->second.isMutableBorrow = false;
        if (it->second.borrowCount > 0) {
            it->second.borrowCount--;
        }
    }
}

bool OwnershipStateTracker::isVariableMoved(const std::string& name) const {
    auto it = variableStates_.find(name);
    return it != variableStates_.end() && it->second.isMoved;
}

bool OwnershipStateTracker::isVariableBorrowed(const std::string& name) const {
    auto it = variableStates_.find(name);
    return it != variableStates_.end() && it->second.isBorrowed;
}

bool OwnershipStateTracker::isVariableMutableBorrowed(const std::string& name) const {
    auto it = variableStates_.find(name);
    return it != variableStates_.end() && it->second.isMutableBorrow;
}

bool OwnershipStateTracker::canMoveVariable(const std::string& name) const {
    auto it = variableStates_.find(name);
    if (it == variableStates_.end()) return true;
    
    return !it->second.isMoved && !it->second.isBorrowed;
}

bool OwnershipStateTracker::canBorrowVariable(const std::string& name, bool mutable) const {
    auto it = variableStates_.find(name);
    if (it == variableStates_.end()) return true;
    
    if (it->second.isMoved) return false;
    
    if (mutable) {
        return !it->second.isBorrowed;
    } else {
        return !it->second.isMutableBorrow;
    }
}

void OwnershipStateTracker::addLifetime(const std::string& variable, const std::string& lifetime) {
    auto it = variableStates_.find(variable);
    if (it != variableStates_.end()) {
        it->second.activeLifetimes.push_back(lifetime);
    }
}

void OwnershipStateTracker::removeLifetime(const std::string& variable, const std::string& lifetime) {
    auto it = variableStates_.find(variable);
    if (it != variableStates_.end()) {
        auto& lifetimes = it->second.activeLifetimes;
        lifetimes.erase(std::remove(lifetimes.begin(), lifetimes.end(), lifetime), lifetimes.end());
    }
}

std::vector<std::string> OwnershipStateTracker::getVariableLifetimes(const std::string& variable) const {
    auto it = variableStates_.find(variable);
    return it != variableStates_.end() ? it->second.activeLifetimes : std::vector<std::string>{};
}

std::vector<std::string> OwnershipStateTracker::getMovedVariables() const {
    std::vector<std::string> moved;
    for (const auto& pair : variableStates_) {
        if (pair.second.isMoved) {
            moved.push_back(pair.first);
        }
    }
    return moved;
}

std::vector<std::string> OwnershipStateTracker::getBorrowedVariables() const {
    std::vector<std::string> borrowed;
    for (const auto& pair : variableStates_) {
        if (pair.second.isBorrowed) {
            borrowed.push_back(pair.first);
        }
    }
    return borrowed;
}

std::vector<std::string> OwnershipStateTracker::getMutableBorrowedVariables() const {
    std::vector<std::string> mutableBorrowed;
    for (const auto& pair : variableStates_) {
        if (pair.second.isMutableBorrow) {
            mutableBorrowed.push_back(pair.first);
        }
    }
    return mutableBorrowed;
}

size_t OwnershipStateTracker::getBorrowCount(const std::string& variable) const {
    auto it = variableStates_.find(variable);
    return it != variableStates_.end() ? it->second.borrowCount : 0;
}

void OwnershipStateTracker::clear() {
    variableStates_.clear();
    scopeVariables_.clear();
    currentScopeLevel_ = 0;
    enterScope(); // Restart with global scope
}

// Global instance for OwnershipUtils
static OwnershipStateTracker globalOwnershipTracker;

// Helper method implementations for OwnershipUtils
BorrowState OwnershipStateTracker::getBorrowState(const std::string& variableName) const {
    auto it = variableStates_.find(variableName);
    if (it == variableStates_.end()) {
        return BorrowState::NOT_BORROWED;
    }
    
    const auto& state = it->second;
    if (state.isMutableBorrow) {
        return BorrowState::MUTABLE_BORROWED;
    } else if (state.isBorrowed) {
        return BorrowState::IMMUTABLE_BORROWED;
    } else {
        return BorrowState::NOT_BORROWED;
    }
}

MoveState OwnershipStateTracker::getMoveState(const std::string& variableName) const {
    auto it = variableStates_.find(variableName);
    if (it == variableStates_.end()) {
        return MoveState::NOT_MOVED;
    }
    return it->second.isMoved ? MoveState::MOVED : MoveState::NOT_MOVED;
}

} // namespace type_checker 