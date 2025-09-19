#pragma once

#include "../pch.h"
#include "../ast/ast.h"
#include "../type/type_checker.h"
#include "../error/error_handler.h"

namespace ir_generator
{
    // Structure to hold class information
    struct ClassInfo
    {
        llvm::StructType* classType;  // LLVM type for this class
        std::vector<std::string> memberNames;  // Names of class members
        llvm::StructType* baseClass;  // Base class type (if any)
    };

    class IRGenerator : public ast::Visitor
    {
    public:
        IRGenerator(llvm::LLVMContext &context, std::unique_ptr<llvm::Module> module,
                    error::ErrorHandler &errorHandler);

        std::unique_ptr<llvm::Module> generate(ast::StmtPtr ast);

        // Visitor methods for statements
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

        // Visitor methods for expressions
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
        
        // Class and OOP support
        std::map<std::string, ClassInfo> classTypes;
        std::map<std::string, llvm::Function*> classMethods;

        // Current value being generated
        llvm::Value *lastValue;

        // Type system
        type_checker::TypeChecker typeChecker;

        // Helper methods
        void declareStdLibFunctions();
        llvm::Function *getStdLibFunction(const std::string &name);
        llvm::AllocaInst *createEntryBlockAlloca(llvm::Function *function,
                                                 const std::string &name,
                                                 llvm::Type *type);
        void createEnvironment();
        void restoreEnvironment();
        void createEmptyList(ast::TypePtr listType);
        void createEmptyDictionary(ast::TypePtr dictType);
        
        // Class and method helpers
        void generateMethod(const std::string& className, llvm::StructType* classType, ast::FunctionStmt* method);

        // Type conversion methods
        llvm::Type *getLLVMType(ast::TypePtr type);
        llvm::FunctionType *getLLVMFunctionType(ast::TypePtr returnType,
                                                const std::vector<ast::Parameter> &params);
    };

} // namespace ir_generator
