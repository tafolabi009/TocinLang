#pragma once

#include "../pch.h"
#include "../ast/ast.h"
#include "../error/error_handler.h"
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>

namespace codegen
{
    /**
     * @brief IR Generator class that translates AST to LLVM IR.
     * 
     * This is a simplified version that can be built while the full implementation
     * is being fixed. It provides basic functionality for the compiler to run.
     */
    class IRGenerator : public ast::Visitor
    {
    public:
        IRGenerator(llvm::LLVMContext &context, std::unique_ptr<llvm::Module> module,
                    error::ErrorHandler &errorHandler);

        /**
         * @brief Generate LLVM IR from an AST.
         * 
         * @param ast The AST to generate code from
         * @return std::unique_ptr<llvm::Module> The generated LLVM module
         */
        std::unique_ptr<llvm::Module> generate(ast::StmtPtr ast);

        // Visitor implementation methods
        void visitBlockStmt(ast::BlockStmt *stmt) override;
        void visitExpressionStmt(ast::ExpressionStmt *stmt) override;
        void visitVariableStmt(ast::VariableStmt *stmt) override;
        void visitFunctionStmt(ast::FunctionStmt *stmt) override;
        void visitReturnStmt(ast::ReturnStmt *stmt) override;
        void visitClassStmt(ast::ClassStmt *stmt) override;
        void visitIfStmt(ast::IfStmt *stmt) override;
        void visitWhileStmt(ast::WhileStmt *stmt) override;
        void visitForStmt(ast::ForStmt *stmt) override;
        void visitMatchStmt(ast::MatchStmt *stmt) override;
        void visitImportStmt(ast::ImportStmt *stmt) override;
        void visitExportStmt(ast::ExportStmt *stmt) override;
        void visitModuleStmt(ast::ModuleStmt *stmt) override;
        void visitBinaryExpr(ast::BinaryExpr *expr) override;
        void visitGroupingExpr(ast::GroupingExpr *expr) override;
        void visitLiteralExpr(ast::LiteralExpr *expr) override;
        void visitUnaryExpr(ast::UnaryExpr *expr) override;
        void visitVariableExpr(ast::VariableExpr *expr) override;
        void visitAssignExpr(ast::AssignExpr *expr) override;
        void visitCallExpr(ast::CallExpr *expr) override;
        void visitGetExpr(ast::GetExpr *expr) override;
        void visitSetExpr(ast::SetExpr *expr) override;
        void visitListExpr(ast::ListExpr *expr) override;
        void visitDictionaryExpr(ast::DictionaryExpr *expr) override;
        void visitLambdaExpr(ast::LambdaExpr *expr) override;
        void visitAwaitExpr(ast::AwaitExpr *expr) override;
        void visitNewExpr(ast::NewExpr *expr) override;
        void visitDeleteExpr(ast::DeleteExpr *expr) override;
        void visitStringInterpolationExpr(ast::StringInterpolationExpr *expr) override;

    private:
        llvm::LLVMContext &context;
        std::unique_ptr<llvm::Module> module;
        llvm::IRBuilder<> builder;
        error::ErrorHandler &errorHandler;
        llvm::Value *lastValue;
        
        /**
         * @brief Create a basic entry point function for the module.
         * 
         * This creates a simple main function that returns 0, so the module
         * is valid and can be verified.
         */
        void createMainFunction();
        
        /**
         * @brief Create a basic print function declaration that can be used in the code.
         */
        void declarePrintFunction();
    };

} // namespace codegen
