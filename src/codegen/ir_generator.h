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
#include <llvm/IR/Value.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/BasicBlock.h>
#include <string>
#include <map>
#include <set>
#include <memory>
#include <vector>

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
        std::map<std::string, llvm::Type *> memberTypes;
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
        std::map<std::string, llvm::AllocaInst *> values;

        Scope(Scope *parent) : parent(parent) {}

        llvm::AllocaInst *lookup(const std::string &name)
        {
            auto it = values.find(name);
            if (it != values.end())
            {
                return it->second;
            }
            else if (parent)
            {
                return parent->lookup(name);
            }
            return nullptr;
        }

        void define(const std::string &name, llvm::AllocaInst *value)
        {
            values[name] = value;
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

        // Freestanding / kernel mode: no libc, no GC, no Tocin runtime. Builtins
        // that need those (print, string ops, collections, file I/O, channels)
        // become compile errors; raw memory + inline asm + arithmetic remain.
        bool freestanding = false;

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
        void visitArrayLiteralExpr(ast::ArrayLiteralExpr *expr) override;
        void visitIndexExpr(ast::IndexExpr *expr) override;
        void visitEnumStmt(ast::EnumStmt *stmt) override;
        void visitTryStmt(ast::TryStmt *stmt) override;
        void visitThrowStmt(ast::ThrowStmt *stmt) override;
        void visitBreakStmt(ast::BreakStmt *stmt) override;
        void visitContinueStmt(ast::ContinueStmt *stmt) override;
        void visitDeferStmt(ast::DeferStmt *stmt) override;
        void visitDestructureStmt(ast::DestructureStmt *stmt) override;
        void visitMoveExpr(void *expr) override;
        void visitGoExpr(void *expr) override;
        void visitRuntimeChannelSendExpr(void *expr) override;
        void visitRuntimeChannelReceiveExpr(void *expr) override;
        void visitRuntimeSelectStmt(void *stmt) override;
        void visitChannelSendExpr(ast::ChannelSendExpr *expr) override;
        void visitChannelReceiveExpr(ast::ChannelReceiveExpr *expr) override;
        void visitSelectStmt(ast::SelectStmt *stmt) override;
        void visitGoStmt(ast::GoStmt* stmt) override;
        void visitTraitStmt(ast::TraitStmt *stmt) override;
        void visitImplStmt(ast::ImplStmt *stmt) override;

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
        type_checker::TypeChecker *typeChecker = nullptr;
        Scope *currentScope = nullptr;
        bool isInAsyncContext = false;
        std::string currentModuleName = "default";
        std::unique_ptr<PatternVisitor> patternVisitor;

        // Symbol tables
        std::map<std::string, llvm::AllocaInst *> namedValues;                     // Variable symbol table
        std::map<std::string, std::string> varClasses;                            // Variable name -> class name
        std::map<std::string, llvm::Type *> varArrayElem;                         // Variable name -> array element LLVM type
        std::map<std::string, std::shared_ptr<ast::FunctionType>> varFuncSig;     // Variable name -> declared function-pointer signature
        std::set<std::string> varIsString;                                        // Variables statically known to hold strings
        std::vector<std::pair<llvm::BasicBlock *, llvm::BasicBlock *>> loopStack; // {continue target, break target} per enclosing loop
        std::vector<std::string> loopLabels;                                      // label per enclosing loop ("" if unlabeled), index-aligned with loopStack
        // Pending cleanups an early `return` must run while unwinding out of
        // try/finally scopes: the finally block (may be null) and whether the
        // exception handler registered for this try must also be popped.
        struct PendingFinally { ast::StmtPtr block; bool popHandler; };
        std::vector<PendingFinally> finallyStack;
        // Function-scoped `defer` statements: each carries a reached-flag (an i1
        // alloca set true when the defer executes) so the cleanup runs at every
        // function-exit path only if it was dynamically reached, in LIFO order.
        struct DeferredStmt { ast::StmtPtr body; llvm::AllocaInst *reached; };
        std::vector<DeferredStmt> deferStack;
        // RAII: class-typed locals initialized directly by a constructor for a
        // class that defines __del__, to be auto-destroyed at every function
        // exit (LIFO, guarded by a reached-flag). Destruction is a deterministic
        // side-effect hook; memory itself remains GC-managed.
        struct OwnedInstance { llvm::AllocaInst *slot; std::string className; llvm::AllocaInst *reached; };
        std::vector<OwnedInstance> destructorStack;
        std::string currentClassName;                                              // Enclosing class while generating a method
        std::string lastExprClassName;                                             // Class name of the most recent expression value
        llvm::Type *lastExprArrayElem = nullptr;                                  // Element type of the most recent array expression
        std::map<std::string, std::shared_ptr<ast::FunctionType>> funcReturnFnType; // function name -> its function-typed return signature
        std::map<std::string, std::string> funcReturnClass;                       // mangled fn/method name -> class name of its return type
        std::map<std::string, int> funcVariadic;                                  // variadic fn name -> fixed (non-variadic) param count
        std::map<std::string, ast::FunctionStmt *> genericFunctions;             // name -> generic function template
        std::map<std::string, ast::ClassStmt *> genericClasses;                  // name -> generic class template
        std::map<const ast::CallExpr *, std::string> genericCtorClass;           // generic constructor call -> mangled class name
        std::map<std::string, int64_t> enumConstants;                           // EnumName.Member and Member -> value
        // Algebraic enum (ADT) variants. An ADT value is a heap buffer
        // [i64 tag][i64 slot0][i64 slot1]... with payloads normalized to 64-bit
        // slots. Plain integer enums are NOT recorded here (they stay scalars).
        struct ADTVariant { std::string enumName; int64_t tag; std::vector<ast::TypePtr> fields; };
        std::map<std::string, ADTVariant> adtVariants;                          // variant name & EnumName.variant -> info
        std::map<std::string, std::vector<std::string>> adtEnumVariants;        // enum name -> ordered variant names
        std::map<std::string, llvm::Type *> typeBindings;                        // active type-parameter bindings during instantiation
        std::map<std::string, llvm::Function *> stdLibFunctions;                   // Standard library functions
        std::map<std::string, ClassInfo> classTypes;                               // Class type information
        std::map<std::string, llvm::Function *> classMethods;                      // Class method table
        std::map<std::string, GenericInstance> genericInstances;                   // Instantiated generic types
        std::map<std::string, std::map<std::string, llvm::Value *>> moduleSymbols; // Module symbols

        // Helper methods
        llvm::AllocaInst *createEntryBlockAlloca(llvm::Function *function, const std::string &name, llvm::Type *type);
        llvm::Type *getLLVMType(ast::TypePtr type);
        llvm::FunctionType *getLLVMFunctionType(ast::TypePtr returnType, const std::vector<ast::Parameter> &params);
        // Build an LLVM function type from a first-class function-type signature.
        llvm::FunctionType *llvmFnTypeOf(const std::shared_ptr<ast::FunctionType> &ft);
        // Recover the signature for an indirect call from the callee expression.
        llvm::FunctionType *recoverCalleeFnType(const ast::ExprPtr &callee);

        // --- Closures -------------------------------------------------------
        // A function value is a heap closure laid out as [ ptr fn ][ caps... ],
        // where fn has the env-first ABI (ptr env, declaredParams...) -> ret.
        // Heap-allocate a closure object and populate fn + captured values.
        llvm::Value *makeClosure(llvm::Function *fn,
                                 const std::vector<llvm::Value *> &caps);
        // A trampoline giving a top-level function the env-first closure ABI,
        // so plain function names can be used as first-class values uniformly.
        llvm::Function *getOrCreateThunk(llvm::Function *target);
        // If v is a bare top-level llvm::Function used as a value, box it into a
        // closure; otherwise return v unchanged.
        llvm::Value *wrapIfRawFunction(llvm::Value *v);
        // Emit any pending finally blocks (innermost first) before a return
        // unwinds out of the enclosing try/finally scopes.
        void runPendingFinally();
        // Emit function-scoped `defer` cleanups (LIFO, each guarded by its
        // reached-flag) at a function-exit point.
        void runDeferred();
        // Emit __del__ calls for owned class instances at a function-exit point.
        void runDestructors();
        // Cache of generated thunks, keyed by the wrapped target function.
        std::map<llvm::Function *, llvm::Function *> thunks;

        // Lightweight, non-emitting type inference used to determine a
        // function's return type when it is not explicitly annotated.
        llvm::Type *inferExprType(const ast::ExprPtr &expr,
                                  const std::map<std::string, llvm::Type *> &localTypes);
        llvm::Type *inferFunctionReturnType(ast::FunctionStmt *stmt);
        void declareStdLibFunctions();
        llvm::Function *getStdLibFunction(const std::string &name);
        llvm::Type *createOpaquePtr(llvm::Type *elementType);

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

        // Variable handling
        bool handleVariableAssignment(ast::AssignExpr *expr, llvm::Value *rhs);
        llvm::AllocaInst *lookupVariable(const std::string &name);
        llvm::Value *getVariable(const std::string &name);
        void addModuleSymbol(const std::string &moduleName, const std::string &symbolName, llvm::Value *value);
        
        // Utility methods
        int getNextId();
        
        // Main function creation
        void createMainFunction();

        // Two-pass codegen: forward-declare prototypes/types so order of
        // top-level declarations does not matter (mutual recursion, etc.).
        void predeclareTopLevel(ast::StmtPtr ast);
        llvm::Function *declareFunctionProto(ast::FunctionStmt *stmt);
        void registerClassType(ast::ClassStmt *stmt);
        llvm::Function *declareMethodProto(const std::string &className,
                                           llvm::StructType *classType,
                                           ast::FunctionStmt *method);

        // String handling
        llvm::Value *convertToString(llvm::Value *value);
        llvm::Value *concatenateStrings(const std::vector<llvm::Value *> &strings);

        // Module system
        llvm::Value *getModuleSymbol(const std::string &moduleName, const std::string &symbolName);
        std::string getQualifiedName(const std::string &moduleName, const std::string &symbolName);

        /**
         * @brief Create a basic print function declaration that can be used in the code.
         */
        void declarePrintFunction();

        // Utility to infer type name from a value (for opaque pointers)
        std::string inferTypeNameFromValue(llvm::Value *value);

        // Determine the class name of an expression (self, local variables of
        // class type, and constructor calls), used for field/method resolution.
        std::string getExprClassName(const ast::ExprPtr &expr);

        // Determine the element LLVM type of an array-valued expression.
        llvm::Type *getArrayElemType(const ast::ExprPtr &expr);

        // True when an expression is statically known to evaluate to a string,
        // so == / != can route to value comparison instead of pointer identity.
        bool isStringExpr(const ast::ExprPtr &expr);

        // If a parameter is typed list<T>/array<T>, remember its element type
        // so indexing the parameter inside the function uses the right type.
        void recordArrayParam(const std::string &name, const ast::TypePtr &type);

        // Option/Result lowering: a heap { i64 tag, i64 payload } object.
        // tag 1 = Some/Ok, tag 0 = Err; None is a null pointer.
        llvm::StructType *optResTy = nullptr;
        llvm::StructType *getOptResType();
        llvm::Value *normalizeToSlot(llvm::Value *v);
        llvm::Value *makeOptRes(int64_t tag, llvm::Value *payload);

        // Construct an algebraic-enum value on the heap: [i64 tag][slot...].
        // `args` are the already-evaluated payload values (each normalized to a
        // 64-bit slot). Returns an opaque pointer to the buffer.
        llvm::Value *makeADT(const ADTVariant &v, const std::vector<llvm::Value *> &args);

        // Construct a tuple on the heap as a flat i64-slot buffer [slot0][slot1]…
        // `slots` are already normalized to 64-bit slots. Returns an opaque ptr.
        llvm::Value *makeTuple(const std::vector<llvm::Value *> &slots);

        // Monomorphize a generic function for a concrete set of type bindings.
        llvm::Function *emitGenericInstance(ast::FunctionStmt *stmt,
                                            const std::map<std::string, llvm::Type *> &bindings);
        // Monomorphize a generic class: build the concrete struct + methods for a
        // set of type bindings and register it. Returns the mangled class name.
        std::string instantiateGenericClass(ast::ClassStmt *stmt,
                                             const std::map<std::string, llvm::Type *> &bindings);
        std::string llvmTypeName(llvm::Type *t);
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
