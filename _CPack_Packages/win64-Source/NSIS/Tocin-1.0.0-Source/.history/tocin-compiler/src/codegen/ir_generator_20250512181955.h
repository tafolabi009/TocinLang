#pragma once

#include "../pch.h"
#include "../ast/ast.h"
#include "../ast/match_stmt.h"
#include "../type/type_checker.h"
#include "../error/error_handler.h"
#include "../runtime/concurrency.h"

namespace ir_generator
{
    // Structure to hold class information
    struct ClassInfo
    {
        llvm::StructType *classType;          // LLVM type for this class
        std::vector<std::string> memberNames; // Names of class members
        llvm::StructType *baseClass;          // Base class type (if any)
    };

    // Forward declaration for the pattern visitor
    class PatternVisitor;

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
        void visitExportStmt(ast::ExportStmt *stmt) override;
        void visitModuleStmt(ast::ModuleStmt *stmt) override;

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
        void visitStringInterpolationExpr(ast::StringInterpolationExpr *expr) override;

        // Pattern matching visitor methods
        void visitWildcardPattern(ast::WildcardPattern *pattern);
        void visitLiteralPattern(ast::LiteralPattern *pattern);
        void visitVariablePattern(ast::VariablePattern *pattern);
        void visitConstructorPattern(ast::ConstructorPattern *pattern);
        void visitTuplePattern(ast::TuplePattern *pattern);
        void visitStructPattern(ast::StructPattern *pattern);
        void visitOrPattern(ast::OrPattern *pattern);

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
        std::map<std::string, llvm::Function *> classMethods;

        // Current value being generated
        llvm::Value *lastValue;

        // Type system
        type_checker::TypeChecker typeChecker;

        // Generic type instantiation tracking
        struct GenericInstance
        {
            std::string baseName;
            std::vector<ast::TypePtr> typeArgs;
            llvm::StructType *instantiatedType;
        };
        std::map<std::string, GenericInstance> genericInstances;

        // Module system state
        std::string currentModuleName;
        std::map<std::string, std::map<std::string, llvm::Value *>> moduleSymbols;

        // Scoping support
        struct Scope {
            std::map<std::string, llvm::AllocaInst*> variables;
            Scope* parent;
            
            Scope(Scope* parent = nullptr) : parent(parent) {}
            
            llvm::AllocaInst* lookup(const std::string& name) {
                auto it = variables.find(name);
                if (it != variables.end()) {
                    return it->second;
                }
                if (parent) {
                    return parent->lookup(name);
                }
                return nullptr;
            }
            
            void define(const std::string& name, llvm::AllocaInst* value) {
                variables[name] = value;
        // Async/await support
        bool isInAsyncContext = false;
        std::unique_ptr<PatternVisitor> patternVisitor;

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
        void generateMethod(const std::string &className, llvm::StructType *classType, ast::FunctionStmt *method);

        // Generic type helpers
        llvm::StructType *instantiateGenericType(const std::string &name,
                                                 const std::vector<ast::TypePtr> &typeArgs);
        llvm::Function *instantiateGenericFunction(ast::FunctionStmt *func,
                                                   const std::vector<ast::TypePtr> &typeArgs);
        std::string mangleGenericName(const std::string &baseName,
                                      const std::vector<ast::TypePtr> &typeArgs);

        // Module system helpers
        void addModuleSymbol(const std::string &moduleName, const std::string &symbolName, llvm::Value *value);
        llvm::Value *getModuleSymbol(const std::string &moduleName, const std::string &symbolName);
        std::string getQualifiedName(const std::string &moduleName, const std::string &symbolName);

        // Async/await helpers
        llvm::StructType *getFutureType(llvm::Type *valueType);
        llvm::StructType *getPromiseType(llvm::Type *valueType);
        llvm::Function *transformAsyncFunction(ast::FunctionStmt *func);

        // Pattern matching helpers
        llvm::Value *generatePatternMatch(llvm::Value *value, ast::PatternPtr pattern);
        bool generatePatternCondition(llvm::Value *value, ast::PatternPtr pattern,
                                      llvm::BasicBlock *successBlock,
                                      llvm::BasicBlock *failBlock);

        // Type conversion methods
        llvm::Type *getLLVMType(ast::TypePtr type);
        llvm::FunctionType *getLLVMFunctionType(ast::TypePtr returnType,
                                                const std::vector<ast::Parameter> &params);
    };

    /**
     * Helper class for visiting pattern types during code generation
     */
    class PatternVisitor
    {
    public:
        PatternVisitor(IRGenerator &generator, llvm::Value *valueToMatch)
            : generator(generator), valueToMatch(valueToMatch), bindingSuccess(false) {}

        bool visitPattern(ast::PatternPtr pattern, llvm::BasicBlock *successBlock, llvm::BasicBlock *failBlock);

        // Return the set of variable bindings created during pattern matching
        const std::map<std::string, llvm::Value *> &getBindings() const { return bindings; }
        bool isSuccess() const { return bindingSuccess; }

    private:
        IRGenerator &generator;
        llvm::Value *valueToMatch;
        std::map<std::string, llvm::Value *> bindings;
        bool bindingSuccess;

        bool visitWildcardPattern(ast::WildcardPattern *pattern, llvm::BasicBlock *successBlock, llvm::BasicBlock *failBlock);
        bool visitLiteralPattern(ast::LiteralPattern *pattern, llvm::BasicBlock *successBlock, llvm::BasicBlock *failBlock);
        bool visitVariablePattern(ast::VariablePattern *pattern, llvm::BasicBlock *successBlock, llvm::BasicBlock *failBlock);
        bool visitConstructorPattern(ast::ConstructorPattern *pattern, llvm::BasicBlock *successBlock, llvm::BasicBlock *failBlock);
        bool visitTuplePattern(ast::TuplePattern *pattern, llvm::BasicBlock *successBlock, llvm::BasicBlock *failBlock);
        bool visitStructPattern(ast::StructPattern *pattern, llvm::BasicBlock *successBlock, llvm::BasicBlock *failBlock);
        bool visitOrPattern(ast::OrPattern *pattern, llvm::BasicBlock *successBlock, llvm::BasicBlock *failBlock);
    };

} // namespace ir_generator
