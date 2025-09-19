#ifndef IR_GENERATOR_H
#define IR_GENERATOR_H

#include "../ast/ast.h"
#include "../error/error_handler.h"
#include "../type/type_checker.h"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Verifier.h>
#include <map>
#include <string>
#include <memory>
#include <vector>

namespace ir_generator {

    /**
     * @brief IR Generator class for generating LLVM IR from AST.
     */
    class IRGenerator : public ast::Visitor {
    public:
        /**
         * @brief Constructs an IR generator.
         * @param context The LLVM context.
         * @param module The LLVM module to populate.
         * @param errorHandler The error handler for reporting issues.
         */
        IRGenerator(llvm::LLVMContext& context, std::unique_ptr<llvm::Module> module, 
                    error::ErrorHandler& errorHandler);

        /**
         * @brief Generates LLVM IR for the AST.
         * @param ast The root statement of the AST.
         * @return The populated LLVM module or nullptr on failure.
         */
        std::unique_ptr<llvm::Module> generate(ast::StmtPtr ast);

        // Visitor implementation for expressions
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
        
        // Visitor implementation for statements
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
        llvm::LLVMContext& context;
        std::unique_ptr<llvm::Module> module;
        llvm::IRBuilder<> builder;
        error::ErrorHandler& errorHandler;
        
        // Symbol table for local variables
        std::map<std::string, llvm::AllocaInst*> namedValues;
        
        // Currently generated value
        llvm::Value* currentValue = nullptr;
        
        // Current function being generated
        llvm::Function* currentFunction = nullptr;
        
        // Return block for the current function
        llvm::BasicBlock* returnBlock = nullptr;
        
        // Map of standard library functions
        std::map<std::string, llvm::Function*> stdLibFunctions;

        // Helper methods
        void declareStdLibFunctions();
        llvm::Function* getStdLibFunction(const std::string& name);
        llvm::AllocaInst* createEntryBlockAlloca(llvm::Function* function, 
                                                const std::string& name, 
                                                llvm::Type* type);
        llvm::Type* getLLVMType(ast::TypePtr type);
        llvm::FunctionType* getLLVMFunctionType(ast::TypePtr returnType, 
                                               const std::vector<ast::Parameter>& params);
        void createEnvironment();
        void restoreEnvironment();
    };

} // namespace ir_generator

#endif // IR_GENERATOR_H
