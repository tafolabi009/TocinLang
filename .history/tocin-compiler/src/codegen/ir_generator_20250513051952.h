#pragma once

#include "../pch.h"
#include "../ast/ast.h"
#include "../ast/match_stmt.h"
#include "../type/type_checker.h"
#include "../error/error_handler.h"
#include "../runtime/concurrency.h"
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>

namespace codegen
{
    // Forward declarations
    class PatternVisitor;

    // Structure to hold class information
    struct ClassInfo
    {
        llvm::StructType *classType;          // LLVM type for this class
        std::vector<std::string> memberNames; // Names of class members
        llvm::StructType *baseClass;          // Base class type (if any)
    };

    // Structure to hold generic instance information
    struct GenericInstance
    {
        std::string baseName;
        std::vector<ast::TypePtr> typeArgs;
        llvm::StructType *instantiatedType;
    };

    // Environment scope for variables
    struct Scope
    {
        Scope *parent;
        std::map<std::string, llvm::AllocaInst *> variables;

        Scope(Scope *parent) : parent(parent) {}

        void define(const std::string &name, llvm::AllocaInst *alloca)
        {
            variables[name] = alloca;
        }

        llvm::AllocaInst *lookup(const std::string &name)
        {
            auto it = variables.find(name);
            if (it != variables.end())
            {
                return it->second;
            }
            if (parent)
            {
                return parent->lookup(name);
            }
            return nullptr;
        }
    };

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
        ~IRGenerator();

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

        // Pattern matching visitor methods
        void visitWildcardPattern(ast::WildcardPattern *pattern);
        void visitLiteralPattern(ast::LiteralPattern *pattern);
        void visitVariablePattern(ast::VariablePattern *pattern);
        void visitConstructorPattern(ast::ConstructorPattern *pattern);
        void visitTuplePattern(ast::TuplePattern *pattern);
        void visitStructPattern(ast::StructPattern *pattern);
        void visitOrPattern(ast::OrPattern *pattern);

        // Exposed for use by PatternVisitor
        llvm::IRBuilder<> builder;
        llvm::Value *lastValue = nullptr;

    private:
        llvm::LLVMContext &context;
        std::unique_ptr<llvm::Module> module;
        llvm::Function *currentFunction = nullptr;
        error::ErrorHandler &errorHandler;
        type_checker::TypeChecker typeChecker;
        Scope *currentScope = nullptr;
        bool isInAsyncContext = false;
        std::string currentModuleName = "default";
        std::unique_ptr<PatternVisitor> patternVisitor;

        // Symbol tables
        std::map<std::string, llvm::AllocaInst *> namedValues;                     // Variable symbol table
        std::map<std::string, llvm::Function *> stdLibFunctions;                   // Standard library functions
        std::map<std::string, ClassInfo> classTypes;                               // Class type information
        std::map<std::string, llvm::Function *> classMethods;                      // Class method table
        std::map<std::string, GenericInstance> genericInstances;                   // Instantiated generic types
        std::map<std::string, std::map<std::string, llvm::Value *>> moduleSymbols; // Module symbols

        // Helper methods
        llvm::AllocaInst *createEntryBlockAlloca(llvm::Function *function, const std::string &name, llvm::Type *type);
        llvm::Type *getLLVMType(ast::TypePtr type);
        llvm::FunctionType *getLLVMFunctionType(ast::TypePtr returnType, const std::vector<ast::Parameter> &params);
        void declareStdLibFunctions();
        llvm::Function *getStdLibFunction(const std::string &name);

        // Scope management
        void enterScope();
        void exitScope();
        void createEnvironment();
        void restoreEnvironment();

        // Generic type handling
        llvm::StructType *instantiateGenericType(const std::string &name, const std::vector<ast::TypePtr> &typeArgs);
        llvm::Function *instantiateGenericFunction(ast::FunctionStmt *func, const std::vector<ast::TypePtr> &typeArgs);
        std::string mangleGenericName(const std::string &baseName, const std::vector<ast::TypePtr> &typeArgs);
        ast::TypePtr substituteTypeParameters(ast::TypePtr type, const std::map<std::string, ast::TypePtr> &substitutions);

        // Async/await support
        llvm::Function *transformAsyncFunction(ast::FunctionStmt *stmt);
        llvm::StructType *getFutureType(llvm::Type *valueType);
        llvm::StructType *getPromiseType(llvm::Type *valueType);

        // Memory management
        void createEmptyList(ast::TypePtr listType);
        void createEmptyDictionary(ast::TypePtr dictType);
        void generateMethod(const std::string &className, llvm::StructType *classType, ast::FunctionStmt *method);

        // Type conversions
        llvm::Value *implicitConversion(llvm::Value *value, llvm::Type *targetType);
        bool canConvertImplicitly(llvm::Type *sourceType, llvm::Type *targetType);
        llvm::Value *createDefaultValue(llvm::Type *type);

        // String handling
        llvm::Value *convertToString(llvm::Value *value);
        llvm::Value *concatenateStrings(const std::vector<llvm::Value *> &strings);

        // Module system
        void addModuleSymbol(const std::string &moduleName, const std::string &symbolName, llvm::Value *value);
        llvm::Value *getModuleSymbol(const std::string &moduleName, const std::string &symbolName);
        std::string getQualifiedName(const std::string &moduleName, const std::string &symbolName);

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

    // Pattern visitor for match statements
    class PatternVisitor
    {
    public:
        PatternVisitor(IRGenerator &generator, llvm::Value *valueToMatch)
            : generator(generator), valueToMatch(valueToMatch),
              lastValue(nullptr), tagMatch(nullptr), bindingSuccess(false) {}

        bool visitPattern(ast::PatternPtr pattern, llvm::BasicBlock *successBlock, llvm::BasicBlock *failBlock);
        bool visitWildcardPattern(ast::WildcardPattern *pattern, llvm::BasicBlock *successBlock, llvm::BasicBlock *failBlock);
        bool visitLiteralPattern(ast::LiteralPattern *pattern, llvm::BasicBlock *successBlock, llvm::BasicBlock *failBlock);
        bool visitVariablePattern(ast::VariablePattern *pattern, llvm::BasicBlock *successBlock, llvm::BasicBlock *failBlock);
        bool visitConstructorPattern(ast::ConstructorPattern *pattern, llvm::BasicBlock *successBlock, llvm::BasicBlock *failBlock);
        bool visitTuplePattern(ast::TuplePattern *pattern, llvm::BasicBlock *successBlock, llvm::BasicBlock *failBlock);
        bool visitStructPattern(ast::StructPattern *pattern, llvm::BasicBlock *successBlock, llvm::BasicBlock *failBlock);
        bool visitOrPattern(ast::OrPattern *pattern, llvm::BasicBlock *successBlock, llvm::BasicBlock *failBlock);

        const std::map<std::string, llvm::Value *> &getBindings() const { return bindings; }

    private:
        IRGenerator &generator;
        llvm::Value *valueToMatch;
        llvm::Value *lastValue;
        llvm::Value *tagMatch;
        bool bindingSuccess;
        std::map<std::string, llvm::Value *> bindings;
    };

} // namespace codegen
