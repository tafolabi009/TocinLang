#include "null_safety.h"
#include <iostream>
#include <sstream>

namespace type_checker {

NullSafetyChecker::NullSafetyChecker(error::ErrorHandler& errorHandler)
    : errorHandler_(errorHandler) {
}

NullSafetyChecker::~NullSafetyChecker() {
}

bool NullSafetyChecker::checkExpression(ast::ExprPtr expr) {
    if (!expr) return true;
    
    try {
        return checkExpressionNullSafety(expr);
    } catch (const std::exception& e) {
        reportNullSafetyError("Expression null safety check failed: " + std::string(e.what()));
        return false;
    }
}

bool NullSafetyChecker::checkStatement(ast::StmtPtr stmt) {
    if (!stmt) return true;
    
    try {
        return checkStatementNullSafety(stmt);
    } catch (const std::exception& e) {
        reportNullSafetyError("Statement null safety check failed: " + std::string(e.what()));
        return false;
    }
}

bool NullSafetyChecker::checkFunction(ast::FunctionDeclPtr /*function*/) {
    // The project uses FunctionStmt in AST; avoid dereferencing incomplete FunctionDecl here.
    return true;
}

bool NullSafetyChecker::isNullableType(ast::TypePtr type) {
    if (!type) return false;
    
    std::string typeStr = type->toString();
    return typeStr.find("?") != std::string::npos || 
           typeStr.find("nullable") != std::string::npos ||
           typeStr.find("null") != std::string::npos;
}

bool NullSafetyChecker::isNonNullType(ast::TypePtr type) {
    if (!type) return false;
    
    return !isNullableType(type);
}

bool NullSafetyChecker::canBeNull(ast::ExprPtr expr) {
    if (!expr) return false;
    
    std::string varName = getVariableName(expr);
    if (!varName.empty()) {
        return nullableVariables_.find(varName) != nullableVariables_.end();
    }
    
    return isNullLiteral(expr);
}

bool NullSafetyChecker::isNullCheck(ast::ExprPtr expr) {
    if (!expr) return false;
    
    // Check for null check expressions (x == null, x != null)
    if (auto binaryExpr = std::dynamic_pointer_cast<ast::BinaryExpr>(expr)) {
        if (binaryExpr->op.type == lexer::TokenType::EQUAL || 
            binaryExpr->op.type == lexer::TokenType::NOT_EQUAL) {
            
            bool leftIsNull = isNullLiteral(binaryExpr->left);
            bool rightIsNull = isNullLiteral(binaryExpr->right);
            
            return leftIsNull || rightIsNull;
        }
    }
    
    return false;
}

bool NullSafetyChecker::isSafeCall(ast::ExprPtr expr) {
    if (!expr) return false;
    
    // Check for safe call expressions (x?.method())
    // The AST has no explicit safe-call flag; return false.
    if (std::dynamic_pointer_cast<ast::CallExpr>(expr)) {
        return false;
    }
    
    // Check for safe property access (x?.property)
    if (std::dynamic_pointer_cast<ast::GetExpr>(expr)) {
        return false;
    }
    
    return false;
}

bool NullSafetyChecker::isElvisOperator(ast::ExprPtr expr) {
    if (!expr) return false;
    
    // Check for Elvis operator expressions (x ?: defaultValue)
    if (auto binaryExpr = std::dynamic_pointer_cast<ast::BinaryExpr>(expr)) {
        if (binaryExpr->op.type == lexer::TokenType::ELVIS) {
            return true;
        }
    }
    
    return false;
}

bool NullSafetyChecker::isNullAssertion(ast::ExprPtr expr) {
    if (!expr) return false;
    
    // Check for null assertion expressions (x!!)
    // No dedicated null-assertion operator in the AST currently.
    (void)expr;
    return false;
    
    return false;
}

ast::TypePtr NullSafetyChecker::makeNullable(ast::TypePtr type) {
    if (!type) return nullptr;
    
    std::string typeStr = type->toString();
    if (isNullableType(type)) return type;
    
    return std::make_shared<ast::SimpleType>(
        lexer::Token(lexer::TokenType::IDENTIFIER, typeStr + "?", "", 0, 0));
}

ast::TypePtr NullSafetyChecker::makeNonNull(ast::TypePtr type) {
    if (!type) return nullptr;
    
    std::string typeStr = type->toString();
    if (isNonNullType(type)) return type;
    
    // Remove nullable markers
    size_t pos = typeStr.find("?");
    if (pos != std::string::npos) {
        typeStr = typeStr.substr(0, pos);
    }
    
    return std::make_shared<ast::SimpleType>(
        lexer::Token(lexer::TokenType::IDENTIFIER, typeStr, "", 0, 0));
}

ast::TypePtr NullSafetyChecker::resolveType(ast::TypePtr type) {
    if (!type) return nullptr;
    
    // Resolve nullable types to their non-nullable versions when appropriate
    if (isNullableType(type)) {
        return makeNonNull(type);
    }
    
    return type;
}

bool NullSafetyChecker::checkSafeCall(ast::ExprPtr expr) {
    if (!expr) return true;
    
    // Check if safe call is used appropriately
    if (isSafeCall(expr)) {
        // Safe call is always valid
        return true;
    }
    
    // Check if unsafe call is made on nullable variable
    // Without object/callee breakdown in CallExpr, skip detailed check.
    
    return true;
}

bool NullSafetyChecker::checkElvisOperator(ast::ExprPtr expr) {
    if (!expr) return true;
    
    // Elvis operator is always valid
    if (isElvisOperator(expr)) {
        return true;
    }
    
    return true;
}

bool NullSafetyChecker::checkNullAssertion(ast::ExprPtr expr) {
    if (!expr) return true;
    
    // Check if null assertion is used appropriately
    if (isNullAssertion(expr)) {
        // No-op placeholder
    }
    
    return true;
}

bool NullSafetyChecker::checkNullCheck(ast::ExprPtr expr) {
    if (!expr) return true;
    
    // Null checks are always valid
    if (isNullCheck(expr)) {
        return true;
    }
    
    return true;
}

bool NullSafetyChecker::analyzeNullFlow(ast::StmtPtr stmt) {
    if (!stmt) return true;
    
    // This is a simplified implementation
    // In a real implementation, you would analyze the control flow for null safety
    
    return true; // Placeholder
}

bool NullSafetyChecker::isNullGuarded(ast::ExprPtr expr) {
    if (!expr) return false;
    
    std::string varName = getVariableName(expr);
    if (varName.empty()) return false;
    
    return nullGuarded_.find(varName) != nullGuarded_.end();
}

bool NullSafetyChecker::isDefinitelyNull(ast::ExprPtr expr) {
    if (!expr) return false;
    
    std::string varName = getVariableName(expr);
    if (varName.empty()) return isNullLiteral(expr);
    
    return definitelyNull_.find(varName) != definitelyNull_.end();
}

bool NullSafetyChecker::isDefinitelyNonNull(ast::ExprPtr expr) {
    if (!expr) return false;
    
    std::string varName = getVariableName(expr);
    if (varName.empty()) return isNotNullLiteral(expr);
    
    return definitelyNonNull_.find(varName) != definitelyNonNull_.end();
}

void NullSafetyChecker::reportNullSafetyError(const std::string& message, ast::Node* node) {
    errorHandler_.reportError(error::ErrorCode::T001_TYPE_MISMATCH, message);
}

bool NullSafetyChecker::checkVariableNullSafety(const std::string& variableName) {
    if (variableName.empty()) return true;
    
    // Check if nullable variable is used safely
    if (nullableVariables_.find(variableName) != nullableVariables_.end()) {
        if (!isNullGuarded(ast::ExprPtr()) && !isSafeCall(ast::ExprPtr())) {
            reportNullSafetyError("Unsafe use of nullable variable: " + variableName);
            return false;
        }
    }
    
    return true;
}

bool NullSafetyChecker::checkExpressionNullSafety(ast::ExprPtr expr) {
    if (!expr) return true;
    
    // Check variable null safety
    std::string varName = getVariableName(expr);
    if (!varName.empty()) {
        if (!checkVariableNullSafety(varName)) {
            return false;
        }
    }
    
    // Check null safety operators
    if (!checkSafeCall(expr)) return false;
    if (!checkElvisOperator(expr)) return false;
    if (!checkNullAssertion(expr)) return false;
    if (!checkNullCheck(expr)) return false;
    
    return true;
}

bool NullSafetyChecker::checkStatementNullSafety(ast::StmtPtr stmt) {
    if (!stmt) return true;
    
    // This is a simplified implementation
    // In a real implementation, you would check different statement types
    
    return true; // Placeholder
}

std::string NullSafetyChecker::getVariableName(ast::ExprPtr expr) {
    if (!expr) return "";
    
    // Extract variable name from expression
    if (auto varExpr = std::dynamic_pointer_cast<ast::VariableExpr>(expr)) {
        return varExpr->name;
    }
    
    // Check for property access (obj.prop)
    if (auto getExpr = std::dynamic_pointer_cast<ast::GetExpr>(expr)) {
        return getVariableName(getExpr->object);
    }
    
    // No IndexExpr in current AST
    
    return "";
}

bool NullSafetyChecker::isVariableExpression(ast::ExprPtr expr) {
    if (!expr) return false;
    
    // Check if expression is a variable reference
    if (std::dynamic_pointer_cast<ast::VariableExpr>(expr)) {
        return true;
    }
    
    // Check for property access
    if (std::dynamic_pointer_cast<ast::GetExpr>(expr)) {
        return true;
    }
    
    // No IndexExpr in current AST
    
    return false;
}

bool NullSafetyChecker::isNullLiteral(ast::ExprPtr expr) {
    if (!expr) return false;
    
    // Check if the expression is a null literal
    if (auto literalExpr = std::dynamic_pointer_cast<ast::LiteralExpr>(expr)) {
        return literalExpr->value == "null" || literalExpr->value == "nil";
    }
    
    return false;
}

bool NullSafetyChecker::isNotNullLiteral(ast::ExprPtr expr) {
    if (!expr) return false;
    
    // Check if the expression is definitely not a null literal
    if (auto literalExpr = std::dynamic_pointer_cast<ast::LiteralExpr>(expr)) {
        return literalExpr->value != "null" && literalExpr->value != "nil";
    }
    
    // Check for other non-null expressions
    if (std::dynamic_pointer_cast<ast::VariableExpr>(expr)) {
        // Variables are generally not null unless explicitly set to null
        return true;
    }
    
    if (std::dynamic_pointer_cast<ast::CallExpr>(expr)) {
        // Function calls are generally not null unless they return nullable types
        return true;
    }
    
    return false;
}

void NullSafetyChecker::markAsNullChecked(const std::string& variableName) {
    nullCheckedVariables_[variableName] = true;
}

void NullSafetyChecker::markAsSafeCall(const std::string& variableName) {
    safeCallVariables_[variableName] = true;
}

void NullSafetyChecker::markAsDefinitelyNull(const std::string& variableName) {
    definitelyNull_.insert(variableName);
}

void NullSafetyChecker::markAsDefinitelyNonNull(const std::string& variableName) {
    definitelyNonNull_.insert(variableName);
}

void NullSafetyChecker::markAsNullGuarded(const std::string& variableName) {
    nullGuarded_.insert(variableName);
}

// NullSafetyUtils implementation
bool NullSafetyUtils::isNullableType(ast::TypePtr type) {
    if (!type) return false;
    
    std::string typeStr = type->toString();
    return typeStr.find("?") != std::string::npos || 
           typeStr.find("nullable") != std::string::npos ||
           typeStr.find("null") != std::string::npos;
}

bool NullSafetyUtils::isNonNullType(ast::TypePtr type) {
    if (!type) return false;
    
    return !isNullableType(type);
}

ast::TypePtr NullSafetyUtils::makeNullable(ast::TypePtr type) {
    if (!type) return nullptr;
    
    std::string typeStr = type->toString();
    if (isNullableType(type)) return type;
    
    return std::make_shared<ast::SimpleType>(
        lexer::Token(lexer::TokenType::IDENTIFIER, typeStr + "?", "", 0, 0));
}

ast::TypePtr NullSafetyUtils::makeNonNull(ast::TypePtr type) {
    if (!type) return nullptr;
    
    std::string typeStr = type->toString();
    if (isNonNullType(type)) return type;
    
    // Remove nullable markers
    size_t pos = typeStr.find("?");
    if (pos != std::string::npos) {
        typeStr = typeStr.substr(0, pos);
    }
    
    return std::make_shared<ast::SimpleType>(
        lexer::Token(lexer::TokenType::IDENTIFIER, typeStr, "", 0, 0));
}

ast::TypePtr NullSafetyUtils::extractInnerType(ast::TypePtr nullableType) {
    if (!nullableType) return nullptr;
    
    return makeNonNull(nullableType);
}

bool NullSafetyUtils::isNullLiteral(ast::ExprPtr expr) {
    if (!expr) return false;
    
    // Check if the expression is a null literal
    if (auto literalExpr = dynamic_cast<ast::LiteralExpr*>(expr.get())) {
        return literalExpr->value == "null" || literalExpr->value == "nil";
    }
    
    return false;
}

bool NullSafetyUtils::isNotNullLiteral(ast::ExprPtr expr) {
    if (!expr) return false;
    
    // Check if the expression is definitely not a null literal
    if (auto literalExpr = dynamic_cast<ast::LiteralExpr*>(expr.get())) {
        return literalExpr->value != "null" && literalExpr->value != "nil";
    }
    
    // Check for other non-null expressions
    if (auto varExpr = dynamic_cast<ast::VariableExpr*>(expr.get())) {
        // Variables are generally not null unless explicitly set to null
        return true;
    }
    
    if (auto callExpr = dynamic_cast<ast::CallExpr*>(expr.get())) {
        // Function calls are generally not null unless they return nullable types
        return true;
    }
    
    return false;
}

bool NullSafetyUtils::isNullCheck(ast::ExprPtr expr) {
    if (!expr) return false;
    
    // Check for null check expressions (x == null, x != null)
    if (auto binaryExpr = dynamic_cast<ast::BinaryExpr*>(expr.get())) {
        if (binaryExpr->op.type == lexer::TokenType::EQUAL || 
            binaryExpr->op.type == lexer::TokenType::NOT_EQUAL) {
            
            bool leftIsNull = isNullLiteral(binaryExpr->left);
            bool rightIsNull = isNullLiteral(binaryExpr->right);
            
            return leftIsNull || rightIsNull;
        }
    }
    
    return false;
}

bool NullSafetyUtils::isSafeCall(ast::ExprPtr expr) {
    if (!expr) return false;
    
    // Check for safe call expressions (x?.method())
    if (dynamic_cast<ast::CallExpr*>(expr.get())) {
        return false;
    }
    
    // Check for safe property access (x?.property)
    if (dynamic_cast<ast::GetExpr*>(expr.get())) {
        return false;
    }
    
    return false;
}

bool NullSafetyUtils::isElvisOperator(ast::ExprPtr expr) {
    if (!expr) return false;
    
    // This is a simplified implementation
    // In a real implementation, you would check for Elvis operator expressions
    
    return false; // Placeholder
}

bool NullSafetyUtils::isNullAssertion(ast::ExprPtr expr) {
    if (!expr) return false;
    
    // This is a simplified implementation
    // In a real implementation, you would check for null assertion expressions
    
    return false; // Placeholder
}

std::string NullSafetyUtils::getSafeCallOperator() {
    return "?.";
}

std::string NullSafetyUtils::getElvisOperator() {
    return "?:";
}

std::string NullSafetyUtils::getNullAssertionOperator() {
    return "!!";
}

std::string NullSafetyUtils::getNullCheckOperator() {
    return "==";
}

std::string NullSafetyUtils::formatNullPointerError(const std::string& variableName) {
    return "Null pointer dereference: " + variableName;
}

std::string NullSafetyUtils::formatNullableAssignmentError(const std::string& variableName) {
    return "Cannot assign nullable value to non-nullable variable: " + variableName;
}

std::string NullSafetyUtils::formatNullCheckError(const std::string& variableName) {
    return "Unsafe null check: " + variableName;
}

std::string NullSafetyUtils::formatSafeCallError(const std::string& variableName) {
    return "Unsafe call on nullable variable: " + variableName;
}

bool NullSafetyUtils::canBeNull(ast::ExprPtr expr) {
    if (!expr) return false;
    
    // This is a simplified implementation
    // In a real implementation, you would check if the expression can be null
    
    return false; // Placeholder
}

bool NullSafetyUtils::isDefinitelyNull(ast::ExprPtr expr) {
    if (!expr) return false;
    
    return isNullLiteral(expr);
}

bool NullSafetyUtils::isDefinitelyNonNull(ast::ExprPtr expr) {
    if (!expr) return false;
    
    return isNotNullLiteral(expr);
}

bool NullSafetyUtils::isNullGuarded(ast::ExprPtr expr) {
    if (!expr) return false;
    
    // This is a simplified implementation
    // In a real implementation, you would check if the expression is null guarded
    
    return false; // Placeholder
}

// NullSafetyFlowAnalyzer implementation
NullSafetyFlowAnalyzer::NullSafetyFlowAnalyzer() : currentScopeLevel_(0) {
    enterScope(); // Start with global scope
}

NullSafetyFlowAnalyzer::~NullSafetyFlowAnalyzer() {
    // Clean up scopes
    while (currentScopeLevel_ > 0) {
        exitScope();
    }
}

bool NullSafetyFlowAnalyzer::analyzeFlow(ast::StmtPtr stmt) {
    if (!stmt) return true;
    
    try {
        // This is a simplified implementation
        // In a real implementation, you would analyze the control flow for null safety
        
        return true; // Placeholder
    } catch (const std::exception& e) {
        return false;
    }
}

bool NullSafetyFlowAnalyzer::analyzeExpressionFlow(ast::ExprPtr expr) {
    if (!expr) return true;
    
    try {
        // This is a simplified implementation
        // In a real implementation, you would analyze expression flow for null safety
        
        return true; // Placeholder
    } catch (const std::exception& e) {
        return false;
    }
}

bool NullSafetyFlowAnalyzer::analyzeConditionalFlow(ast::ExprPtr condition, ast::StmtPtr thenStmt, ast::StmtPtr elseStmt) {
    try {
        // This is a simplified implementation
        // In a real implementation, you would analyze conditional flow for null safety
        
        return true; // Placeholder
    } catch (const std::exception& e) {
        return false;
    }
}

void NullSafetyFlowAnalyzer::enterScope() {
    currentScopeLevel_++;
    scopeVariables_.push_back(std::unordered_set<std::string>());
}

void NullSafetyFlowAnalyzer::exitScope() {
    if (currentScopeLevel_ > 0) {
        // Remove variables from current scope
        if (!scopeVariables_.empty()) {
            for (const auto& varName : scopeVariables_.back()) {
                flowStates_.erase(varName);
            }
            scopeVariables_.pop_back();
        }
        currentScopeLevel_--;
    }
}

void NullSafetyFlowAnalyzer::addVariable(const std::string& name) {
    if (currentScopeLevel_ > 0 && !scopeVariables_.empty()) {
        scopeVariables_.back().insert(name);
        flowStates_[name] = FlowState();
    }
}

void NullSafetyFlowAnalyzer::removeVariable(const std::string& name) {
    flowStates_.erase(name);
}

bool NullSafetyFlowAnalyzer::isVariableNullGuarded(const std::string& name) const {
    auto it = flowStates_.find(name);
    return it != flowStates_.end() && it->second.isNullGuarded;
}

bool NullSafetyFlowAnalyzer::isVariableDefinitelyNull(const std::string& name) const {
    auto it = flowStates_.find(name);
    return it != flowStates_.end() && it->second.isDefinitelyNull;
}

bool NullSafetyFlowAnalyzer::isVariableDefinitelyNonNull(const std::string& name) const {
    auto it = flowStates_.find(name);
    return it != flowStates_.end() && it->second.isDefinitelyNonNull;
}

bool NullSafetyFlowAnalyzer::isVariableNullable(const std::string& name) const {
    auto it = flowStates_.find(name);
    return it != flowStates_.end() && it->second.isNullable;
}

void NullSafetyFlowAnalyzer::markAsNullGuarded(const std::string& name) {
    auto it = flowStates_.find(name);
    if (it != flowStates_.end()) {
        it->second.isNullGuarded = true;
    }
}

void NullSafetyFlowAnalyzer::markAsDefinitelyNull(const std::string& name) {
    auto it = flowStates_.find(name);
    if (it != flowStates_.end()) {
        it->second.isDefinitelyNull = true;
        it->second.isDefinitelyNonNull = false;
    }
}

void NullSafetyFlowAnalyzer::markAsDefinitelyNonNull(const std::string& name) {
    auto it = flowStates_.find(name);
    if (it != flowStates_.end()) {
        it->second.isDefinitelyNonNull = true;
        it->second.isDefinitelyNull = false;
    }
}

void NullSafetyFlowAnalyzer::markAsNullable(const std::string& name) {
    auto it = flowStates_.find(name);
    if (it != flowStates_.end()) {
        it->second.isNullable = true;
    }
}

void NullSafetyFlowAnalyzer::clear() {
    flowStates_.clear();
    scopeVariables_.clear();
    currentScopeLevel_ = 0;
    enterScope(); // Restart with global scope
}

} // namespace type_checker 