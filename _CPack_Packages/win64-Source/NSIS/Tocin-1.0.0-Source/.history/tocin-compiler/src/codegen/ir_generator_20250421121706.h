#ifndef IR_GENERATOR_H
#define IR_GENERATOR_H

#include "../ast/ast.h"
#include "../error/error_handler.h"
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <absl/container/flat_hash_map.h>
#include <absl/container/inlined_vector.h>
#include <memory>
#include <string>

namespace ir_generator {

    /**
     * @brief IR generator for converting AST to LLVM IR.
     */
    class IRGenerator : public ast::Visitor {
    public:
        /**
         * @brief Constructs an IR generator.
         * @param context The LLVM context.
         * @param module The LLVM module.
         * @param errorHandler The error handler.
         */
        IRGenerator(llvm::LLVMContext& context, std::unique_ptr<llvm::Module> module,
            error::ErrorHandler& errorHandler);

        /**
         * @brief Generates IR for the given AST.
         * @param stmt The root statement.
         * @return The generated LLVM module.
         */
        std::unique_ptr<llvm::Module> generate(ast::StmtPtr stmt);

        void visitBinaryExpr(ast::BinaryExpr* expr) override;
        void visitGroupingExpr(ast::GroupingExpr* expr) override;
        void visitLiteralExpr(ast::LiteralExpr* expr) override;
        void visitUnaryExpr(ast::UnaryExpr* expr) override;
        void visitVariableExpr(ast::VariableExpr* expr) override;
        void visitAssignExpr(ast::AssignExpr* expr) override;
        void visitCallExpr(ast::CallExpr* expr) override;
        void visitGetExpr(ast::GetExpr* expr) override;
        void visitSetExpr(ast::SetExpr* expr) override;
        void visitListExpr(ast::ListExpr* expr) override;
        void visitDictionaryExpr(ast::DictionaryExpr* expr) override;
        void visitLambdaExpr(ast::LambdaExpr* expr) override;
        void visitAwaitExpr(ast::AwaitExpr* expr) override;
        void visitExpressionStmt(ast::ExpressionStmt* stmt) override;
        void visitVariableStmt(ast::VariableStmt* stmt) override;
        void visitBlockStmt(ast::BlockStmt* stmt) override;
        void visitIfStmt(ast::IfStmt* stmt) override;
        void visitWhileStmt(ast::WhileStmt* stmt) override;
        void visitForStmt(ast::ForStmt* stmt) override;
        void visitFunctionStmt(ast::FunctionStmt* stmt) override;
        void visitReturnStmt(ast::ReturnStmt* stmt) override;
        void visitClassStmt(ast::ClassStmt* stmt) override;
        void visitImportStmt(ast::ImportStmt* stmt) override;
        void visitMatchStmt(ast::MatchStmt* stmt) override;

    private:
        llvm::Type* getLLVMType(ast::TypePtr type);
        llvm::Value* currentValue_;
        llvm::Function* currentFunction_;
        llvm::LLVMContext& context_;
        std::unique_ptr<llvm::Module> module_;
        llvm::IRBuilder<> builder_;
        absl::flat_hash_map<std::string, llvm::Value*> namedValues_;
        absl::flat_hash_map<std::string, ast::TypePtr> typeMap_;
        error::ErrorHandler& errorHandler_;
        bool inAsyncContext_ = false;

        void createAsyncFunction(ast::FunctionStmt* stmt);
        llvm::Value* createFFITypeValidation(ast::TypePtr type, llvm::Value* value);
    };

} // namespace ir_generator

#endif // IR_GENERATOR_H