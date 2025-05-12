#pragma once

#include "../pch.h"
#include "../ast/ast.h"
#include "../type/type_checker.h"
#include "../error/error_handler.h"

namespace ir_generator
{

    class IRGenerator : public ast::Visitor
    {
    public:
        IRGenerator(llvm::LLVMContext &context, std::unique_ptr<llvm::Module> module,
                    error::ErrorHandler &errorHandler);

        std::unique_ptr<llvm::Module> generate(ast::StmtPtr ast);

        // Visitor methods for statements
        void visitBlockStmt(ast::BlockStmt *stmt) override;
        void visitExpressionStmt(ast::ExpressionStmt *stmt) override;
        void visitPrintStmt(ast::PrintStmt *stmt) override;
        void visitVarStmt(ast::VarStmt *stmt) override;
        void visitFunctionStmt(ast::FunctionStmt *stmt) override;
        void visitReturnStmt(ast::ReturnStmt *stmt) override;
        void visitClassStmt(ast::ClassStmt *stmt) override;
        void visitIfStmt(ast::IfStmt *stmt) override;
        void visitWhileStmt(ast::WhileStmt *stmt) override;
        void visitForStmt(ast::ForStmt *stmt) override;
        void visitMatchStmt(ast::MatchStmt *stmt) override;

        // Visitor methods for expressions
        void visitBinaryExpr(ast::BinaryExpr *expr) override;
        void visitGroupingExpr(ast::GroupingExpr *expr) override;
        void visitLiteralExpr(ast::LiteralExpr *expr) override;
        void visitUnaryExpr(ast::UnaryExpr *expr) override;
        void visitVariableExpr(ast::VariableExpr *expr) override;
        void visitAssignExpr(ast::AssignExpr *expr) override;
        void visitLogicalExpr(ast::LogicalExpr *expr) override;
        void visitCallExpr(ast::CallExpr *expr) override;
        void visitGetExpr(ast::GetExpr *expr) override;
        void visitSetExpr(ast::SetExpr *expr) override;
        void visitThisExpr(ast::ThisExpr *expr) override;
        void visitSuperExpr(ast::SuperExpr *expr) override;
        void visitLambdaExpr(ast::LambdaExpr *expr) override;
        void visitArrayExpr(ast::ArrayExpr *expr) override;
        void visitIndexExpr(ast::IndexExpr *expr) override;
        void visitOptionExpr(ast::OptionExpr *expr) override;
        void visitResultExpr(ast::ResultExpr *expr) override;

    private:
        // LLVM context and state
        llvm::LLVMContext &context;
        std::unique_ptr<llvm::Module> module;
        llvm::IRBuilder<> builder;
        llvm::Function *currentFunction;

        // Error handling
        error::ErrorHandler &errorHandler;

        // Symbol tables
        std::map<std::string, llvm::AllocaInst *> namedValues;
        std::map<std::string, llvm::Function *> stdLibFunctions;

        // Current value being generated
        llvm::Value *lastValue;

        // Type system
        type::TypeChecker typeChecker;

        // Helper methods
        void declareStdLibFunctions();
        llvm::Function *getStdLibFunction(const std::string &name);
        llvm::AllocaInst *createEntryBlockAlloca(llvm::Function *function,
                                                 const std::string &name,
                                                 llvm::Type *type);
        void createEnvironment();
        void restoreEnvironment();

        // Type conversion methods
        llvm::Type *getLLVMType(ast::TypePtr type);
        llvm::Type *getLLVMType(std::shared_ptr<type::Type> tocinType);
        llvm::FunctionType *getLLVMFunctionType(ast::TypePtr returnType,
                                                const std::vector<ast::Parameter> &params);
    };

} // namespace ir_generator
