#include "type_checker.h"
#include <stdexcept>
#include "result_option.h"

namespace type_checker
{
    // Remove stale static consts; use ast::OptionType/ResultType toString instead

    void TypeChecker::visitArrayLiteralExpr(ast::ArrayLiteralExpr *expr)
    {
        // Check the type of each element in the array
        ast::TypePtr elementType = nullptr;

        for (const auto &element : expr->elements)
        {
            element->accept(*this);
            if (elementType == nullptr)
            {
                elementType = currentType_;
            }
            else if (!isAssignable(currentType_, elementType))
            {
                // If new element type doesn't match previous, either coerce or report error
                if (isAssignable(elementType, currentType_))
                {
                    elementType = currentType_; // Widen the type
                }
                else
                {
                    errorHandler_.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                              "Array literal has inconsistent element types",
                                              std::string(expr->token.filename), expr->token.line, expr->token.column,
                                              error::ErrorSeverity::ERROR);
                    break;
                }
            }
        }

        if (elementType == nullptr)
        {
            // Empty array, default to int for now
            elementType = std::make_shared<ast::SimpleType>(
                lexer::Token(lexer::TokenType::IDENTIFIER, "int", "", 0, 0));
        }

        // Create an array type with the determined element type
        currentType_ = std::make_shared<ast::GenericType>(
            expr->token,
            "array",
            std::vector<ast::TypePtr>{elementType});
    }

    void TypeChecker::visitMoveExpr(void *expr)
    {
        // Not supported; no-op
        (void)expr;
        currentType_ = nullptr;
    }

    void TypeChecker::visitGoExpr(void *expr)
    {
        (void)expr;
        currentType_ = std::make_shared<ast::SimpleType>(lexer::Token(lexer::TokenType::IDENTIFIER, "void", "", 0, 0));
    }

    void TypeChecker::visitRuntimeChannelSendExpr(void *expr)
    {
        // Type check channel send (channel <- value)
        auto sendExpr = static_cast<ast::ChannelSendExpr*>(expr);
        if (!sendExpr) {
            currentType_ = nullptr;
            return;
        }

        // Check channel type
        if (sendExpr->channel) {
            sendExpr->channel->accept(*this);
            auto channelType = currentType_;
            
            // Check value type
            if (sendExpr->value) {
                sendExpr->value->accept(*this);
                auto valueType = currentType_;
                
                // Verify the value type matches the channel element type
                if (channelType && valueType) {
                    auto elementType = getChannelElementType(channelType);
                    if (elementType && !typesCompatible(valueType, elementType)) {
                        errorHandler_.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                                "Cannot send value of type " + valueType->toString() + 
                                                " to channel of type " + channelType->toString());
                    }
                }
            }
        }
        
        // Channel send returns void
        currentType_ = std::make_shared<ast::SimpleType>(
            lexer::Token(lexer::TokenType::IDENTIFIER, "void", "", 0, 0));
    }

    void TypeChecker::visitRuntimeChannelReceiveExpr(void *expr)
    {
        // Type check channel receive (<-channel)
        auto receiveExpr = static_cast<ast::ChannelReceiveExpr*>(expr);
        if (!receiveExpr) {
            currentType_ = nullptr;
            return;
        }

        // Check channel type
        if (receiveExpr->channel) {
            receiveExpr->channel->accept(*this);
            auto channelType = currentType_;
            
            if (channelType) {
                // The type of a receive expression is the channel's element type
                currentType_ = getChannelElementType(channelType);
            } else {
                currentType_ = nullptr;
            }
        } else {
            currentType_ = nullptr;
        }
    }

    void TypeChecker::visitRuntimeSelectStmt(void *stmt)
    {
        // Type check select statement (concurrency)
        auto selectStmt = static_cast<ast::SelectStmt*>(stmt);
        if (!selectStmt) {
            currentType_ = nullptr;
            return;
        }

        // Check all cases in the select statement
        for (const auto& caseStmt : selectStmt->cases) {
            if (caseStmt.body) {
                caseStmt.body->accept(*this);
            }
        }
        
        // Select statement returns void
        currentType_ = std::make_shared<ast::SimpleType>(
            lexer::Token(lexer::TokenType::IDENTIFIER, "void", "", 0, 0));
    }

    // Implementation for channel-related visitor methods
    void TypeChecker::visitChannelSendExpr(ast::ChannelSendExpr *expr)
    {
        // Type check channel send (channel <- value)
        if (expr->channel) expr->channel->accept(*this);
        if (expr->value) expr->value->accept(*this);
        
        // Channel send returns void
        currentType_ = std::make_shared<ast::SimpleType>(
            lexer::Token(lexer::TokenType::IDENTIFIER, "void", "", 0, 0));
    }

    void TypeChecker::visitChannelReceiveExpr(ast::ChannelReceiveExpr *expr)
    {
        // Type check channel receive (<-channel)
        if (expr->channel) {
            expr->channel->accept(*this);
            auto channelType = currentType_;
            
            if (channelType) {
                // The type of a receive expression is the channel's element type
                currentType_ = getChannelElementType(channelType);
            } else {
                currentType_ = nullptr;
            }
        } else {
            currentType_ = nullptr;
        }
    }

    void TypeChecker::visitSelectStmt(ast::SelectStmt *stmt)
    {
        // Type check select statement (concurrency)
        // In practice, check all cases and ensure they are valid channel operations
        currentType_ = nullptr;
    }

    // Check if one type can be assigned to another
    bool TypeChecker::isAssignable(ast::TypePtr from, ast::TypePtr to)
    {
        if (!from || !to)
        {
            return false;
        }

        // Same type is always assignable
        if (from->equals(to))
        {
            return true;
        }

        // Handle basic type conversions
        if (auto fromBasic = std::dynamic_pointer_cast<ast::BasicType>(from))
        {
            if (auto toBasic = std::dynamic_pointer_cast<ast::BasicType>(to))
            {
                // Allow int to float conversion
                if (fromBasic->getKind() == ast::TypeKind::INT && toBasic->getKind() == ast::TypeKind::FLOAT)
                {
                    return true;
                }
            }
        }

        // Handle generic types
        if (auto fromGeneric = std::dynamic_pointer_cast<ast::GenericType>(from))
        {
            if (auto toGeneric = std::dynamic_pointer_cast<ast::GenericType>(to))
            {
                if (fromGeneric->name == toGeneric->name)
                {
                    // Check if type arguments are assignable
                    if (fromGeneric->typeArguments.size() == toGeneric->typeArguments.size())
                    {
                        for (size_t i = 0; i < fromGeneric->typeArguments.size(); ++i)
                        {
                            if (!isAssignable(fromGeneric->typeArguments[i], toGeneric->typeArguments[i]))
                            {
                                return false;
                            }
                        }
                        return true;
                    }
                }
            }
        }

        return false;
    }

    // Constructor for TypeChecker
    TypeChecker::TypeChecker(error::ErrorHandler &errorHandler, tocin::compiler::CompilationContext &context, FeatureManager *featureManager)
        : errorHandler_(errorHandler), compilationContext_(context), featureManager_(featureManager)
    {
    }

    // Check method for type checking statements
    ast::TypePtr TypeChecker::check(ast::StmtPtr stmt)
    {
        if (!stmt)
        {
            return nullptr;
        }

        try
        {
            stmt->accept(*this);
            return currentType_;
        }
        catch (const std::exception &e)
        {
            errorHandler_.reportError(
                error::ErrorCode::T001_TYPE_MISMATCH,
                "Type checking error: " + std::string(e.what()),
                "", 0, 0, error::ErrorSeverity::ERROR);
            return nullptr;
        }
    }

    // Missing virtual function implementations
    void TypeChecker::visitLiteralExpr(ast::LiteralExpr *expr)
    {
        // Set current type based on literal type
        switch (expr->literalType)
        {
        case ast::LiteralExpr::LiteralType::INTEGER:
            currentType_ = std::make_shared<ast::BasicType>(ast::TypeKind::INT);
            break;
        case ast::LiteralExpr::LiteralType::FLOAT:
            currentType_ = std::make_shared<ast::BasicType>(ast::TypeKind::FLOAT);
            break;
        case ast::LiteralExpr::LiteralType::BOOLEAN:
            currentType_ = std::make_shared<ast::BasicType>(ast::TypeKind::BOOL);
            break;
        case ast::LiteralExpr::LiteralType::STRING:
            currentType_ = std::make_shared<ast::BasicType>(ast::TypeKind::STRING);
            break;
        case ast::LiteralExpr::LiteralType::NIL:
            currentType_ = std::make_shared<ast::BasicType>(ast::TypeKind::VOID);
            break;
        default:
            currentType_ = std::make_shared<ast::BasicType>(ast::TypeKind::VOID);
            break;
        }
    }

    void TypeChecker::visitUnaryExpr(ast::UnaryExpr *expr)
    {
        if (expr->right) expr->right->accept(*this);
        // For now, keep the same type as the operand
    }

    void TypeChecker::visitAssignExpr(ast::AssignExpr *expr)
    {
        if (expr->value) expr->value->accept(*this);
        // For now, keep the same type as the value
    }

    void TypeChecker::visitCallExpr(ast::CallExpr *expr)
    {
        if (expr->callee) expr->callee->accept(*this);
        // For now, return void for function calls
        currentType_ = std::make_shared<ast::BasicType>(ast::TypeKind::VOID);
    }

    void TypeChecker::visitGetExpr(ast::GetExpr *expr)
    {
        if (expr->object) expr->object->accept(*this);
        // For now, return void for property access
        currentType_ = std::make_shared<ast::BasicType>(ast::TypeKind::VOID);
    }

    void TypeChecker::visitSetExpr(ast::SetExpr *expr)
    {
        if (expr->object) expr->object->accept(*this);
        if (expr->value) expr->value->accept(*this);
        // For now, return void for property assignment
        currentType_ = std::make_shared<ast::BasicType>(ast::TypeKind::VOID);
    }

    void TypeChecker::visitListExpr(ast::ListExpr *expr)
    {
        // For now, return a generic list type
        currentType_ = std::make_shared<ast::GenericType>(
            expr->token, "List", std::vector<ast::TypePtr>{});
    }

    void TypeChecker::visitDictionaryExpr(ast::DictionaryExpr *expr)
    {
        // For now, return a generic dictionary type
        currentType_ = std::make_shared<ast::GenericType>(
            expr->token, "Dict", std::vector<ast::TypePtr>{});
    }

    void TypeChecker::visitLambdaExpr(ast::LambdaExpr *expr)
    {
        // For now, return a generic function type
        currentType_ = std::make_shared<ast::GenericType>(
            expr->token, "Function", std::vector<ast::TypePtr>{});
    }

    void TypeChecker::visitDeleteExpr(ast::DeleteExpr *expr)
    {
        if (expr->getExpr()) expr->getExpr()->accept(*this);
        // Delete expressions return void
        currentType_ = std::make_shared<ast::BasicType>(ast::TypeKind::VOID);
    }

    void TypeChecker::visitStringInterpolationExpr(ast::StringInterpolationExpr *expr)
    {
        // String interpolation returns a string
        currentType_ = std::make_shared<ast::BasicType>(ast::TypeKind::STRING);
    }

    void TypeChecker::visitVariableStmt(ast::VariableStmt *stmt)
    {
        if (stmt->initializer) stmt->initializer->accept(*this);
        // Variable statements don't have a type
        currentType_ = nullptr;
    }

    void TypeChecker::visitIfStmt(ast::IfStmt *stmt)
    {
        if (stmt->condition) stmt->condition->accept(*this);
        if (stmt->thenBranch) stmt->thenBranch->accept(*this);
        if (stmt->elseBranch) stmt->elseBranch->accept(*this);
        // If statements don't have a type
        currentType_ = nullptr;
    }

    void TypeChecker::visitWhileStmt(ast::WhileStmt *stmt)
    {
        if (stmt->condition) stmt->condition->accept(*this);
        if (stmt->body) stmt->body->accept(*this);
        // While statements don't have a type
        currentType_ = nullptr;
    }

    void TypeChecker::visitForStmt(ast::ForStmt *stmt)
    {
        if (stmt->iterable) stmt->iterable->accept(*this);
        if (stmt->body) stmt->body->accept(*this);
        // For statements don't have a type
        currentType_ = nullptr;
    }

    void TypeChecker::visitFunctionStmt(ast::FunctionStmt *stmt)
    {
        // Function statements don't have a type
        currentType_ = nullptr;
    }

    void TypeChecker::visitReturnStmt(ast::ReturnStmt *stmt)
    {
        if (stmt->value) stmt->value->accept(*this);
        // Return statements don't have a type
        currentType_ = nullptr;
    }

    void TypeChecker::visitClassStmt(ast::ClassStmt *stmt)
    {
        // Class statements don't have a type
        currentType_ = nullptr;
    }

    void TypeChecker::visitBinaryExpr(ast::BinaryExpr *expr)
    {
        // Visit left and right operands
        if (expr->left) expr->left->accept(*this);
        if (expr->right) expr->right->accept(*this);
        
        // For now, just set lastValue to a constant
        currentType_ = std::make_shared<ast::BasicType>(ast::TypeKind::INT);
    }

    void TypeChecker::visitGroupingExpr(ast::GroupingExpr *expr)
    {
        if (expr->expression) expr->expression->accept(*this);
    }

    void TypeChecker::visitVariableExpr(ast::VariableExpr *expr)
    {
        // Look up variable in current scope
        currentType_ = std::make_shared<ast::BasicType>(ast::TypeKind::INT);
    }

    void TypeChecker::visitExpressionStmt(ast::ExpressionStmt *stmt)
    {
        if (stmt->expression) stmt->expression->accept(*this);
    }

    void TypeChecker::visitBlockStmt(ast::BlockStmt *stmt)
    {
        for (auto &statement : stmt->statements)
        {
            if (statement) statement->accept(*this);
        }
    }

    void TypeChecker::visitImportStmt(ast::ImportStmt *stmt)
    {
        // Import statements are handled at compile time, not runtime
    }

    void TypeChecker::visitMatchStmt(ast::MatchStmt *stmt)
    {
        // Basic match statement implementation
        if (stmt->value) stmt->value->accept(*this);
        
        // For now, just visit the first case
        if (!stmt->cases.empty() && stmt->cases[0].second)
        {
            stmt->cases[0].second->accept(*this);
        }
    }

    void TypeChecker::visitNewExpr(ast::NewExpr *expr)
    {
        // Basic new expression - allocate memory
        ast::TypePtr type = expr->getType();
        if (type)
        {
            currentType_ = type;
        }
    }

    void TypeChecker::visitExportStmt(ast::ExportStmt *stmt)
    {
        // Export statements are handled at compile time
    }

    void TypeChecker::visitModuleStmt(ast::ModuleStmt *stmt)
    {
        // Module statements define module scope
        // Visit all statements in the module body
        for (auto &statement : stmt->body)
        {
            if (statement) statement->accept(*this);
        }
    }

    void TypeChecker::visitAwaitExpr(ast::AwaitExpr *expr)
    {
        // Basic await expression - just visit the inner expression
        if (expr->expression) expr->expression->accept(*this);
    }

    void TypeChecker::visitGoStmt(ast::GoStmt* stmt) {
        // Type check goroutine launch (go statement)
        if (stmt->expression) stmt->expression->accept(*this);
        // In practice, check that the expression is a function call and is goroutine-safe
        currentType_ = nullptr;
    }

    bool TypeChecker::canRunAsGoroutine(ast::ExprPtr expr) {
        // Check if an expression can be run as a goroutine
        // For now, allow any function call
        if (auto callExpr = std::dynamic_pointer_cast<ast::CallExpr>(expr)) {
            // Type check the function call
            callExpr->accept(*this);
            
            // Get the function type
            ast::TypePtr funcType = callExpr->getType();
            if (!funcType) {
                return false;
            }
            
            // Verify it's a function type
            if (auto functionType = std::dynamic_pointer_cast<ast::FunctionType>(funcType)) {
                // For now, allow any function to be run as a goroutine
                // In the future, we could add restrictions (e.g., no variadic functions, etc.)
                return true;
            }
        }
        
        return false;
    }

    bool TypeChecker::validateGoroutineLaunch(ast::ExprPtr function, const std::vector<ast::ExprPtr>& arguments) {
        // Validate that a function can be launched as a goroutine with the given arguments
        if (!canRunAsGoroutine(function)) {
            return false;
        }
        
        // Type check the function call with arguments
        if (auto callExpr = std::dynamic_pointer_cast<ast::CallExpr>(function)) {
            // Verify argument types match function signature
            // This would be done during normal function call type checking
            return true;
        }
        
        return false;
    }

    void TypeChecker::visitTraitStmt(ast::TraitStmt *stmt) {
        // Type check trait statement
        // Register the trait in the trait manager if available
        if (featureManager_) {
            // In practice, would register the trait with the trait manager
            // featureManager_->traitManager.registerTrait(stmt->name, stmt);
        }
        
        // Type check all method signatures in the trait
        for (const auto& method : stmt->methods) {
            if (method) {
                method->accept(*this);
            }
        }
        
        // Trait statements don't have a type
        currentType_ = nullptr;
    }

    void TypeChecker::visitImplStmt(ast::ImplStmt *stmt) {
        // Type check implementation statement
        // Verify that the implementation satisfies the trait requirements
        if (featureManager_) {
            // In practice, would verify the implementation against the trait
            // auto trait = featureManager_->traitManager.getTrait(stmt->traitName);
            // if (trait) {
            //     featureManager_->traitManager.verifyImplementation(trait.get(), stmt);
            // }
        }
        
        // Type check all method implementations
        for (const auto& method : stmt->methods) {
            if (method) {
                method->accept(*this);
            }
        }
        
        // Implementation statements don't have a type
        currentType_ = nullptr;
    }

    // Helper methods for type checking
    // isMovableType removed

    // getChannelElementType removed

    // typesCompatible removed

} // namespace type_checker 