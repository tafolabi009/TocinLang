#ifndef IR_GENERATOR_H
#define IR_GENERATOR_H

#include "../ast/ast.h"
#include "../ast/ast_visitor.h"
#include "../error/error_handler.h" // Include ErrorHandler
#include "../type/type_checker.h"   // Include TypeChecker forward declaration needed

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Function.h" // Include Function header
#include "llvm/IR/Verifier.h" // Include Verifier header
#include <map>
#include <string>
#include <memory>
#include <vector> // Include vector for storing arguments

// Forward declarations (if not fully included)
namespace Tocin
{
    class Type; // Forward declare Tocin::Type if header not included
    // Add forward declarations for all AST node types visited
    class VariableDecl;
    class FunctionStmt;
    class ReturnStmt;
    class IfStmt;
    class WhileStmt;
    class BlockStmt;
    class ExpressionStmt;
    class LiteralExpr;
    class UnaryExpr;
    class BinaryExpr;
    class CallExpr;
    class VariableExpr;
    class AssignmentExpr;
    class LogicalExpr;
    class GroupingExpr;
    // Add others as needed: ClassStmt, GetExpr, SetExpr, ListExpr, etc.
}

namespace codegen
{

    using namespace Tocin; // Use Tocin namespace for AST nodes

    // The IRGenerator class traverses the AST (after type checking)
    // and generates LLVM Intermediate Representation (IR).
    class IRGenerator : public ASTVisitor
    {
    private:
        llvm::LLVMContext &context;        // LLVM Context
        llvm::IRBuilder<> builder;         // LLVM IR Builder
        llvm::Module &module;              // LLVM Module being built
        error::ErrorHandler &errorHandler; // For reporting codegen errors
        TypeChecker &typeChecker;          // To get type information for nodes

        // Symbol Table for local variables/parameters within the current function scope
        // Maps variable name to its memory location (AllocaInst)
        // Needs proper scoping (e.g., stack of maps) for nested blocks
        std::map<std::string, llvm::AllocaInst *> namedValues;

        // Stores the last generated LLVM value (result of visiting an expression)
        llvm::Value *lastValue = nullptr;

        // --- Added for Standard Library ---
        // Map from Tocin standard library function name (e.g., "println")
        // to the corresponding declared LLVM Function.
        std::map<std::string, llvm::Function *> standardLibraryFunctions;

        /**
         * @brief Declares standard library functions in the LLVM module.
         * Called once during IRGenerator initialization.
         */
        void declareStandardLibraryFunctions();

        /**
         * @brief Retrieves a previously declared standard library LLVM function.
         * @param name The Tocin name of the standard library function (e.g., "println").
         * @return Pointer to the llvm::Function or nullptr if not found (reports error).
         */
        llvm::Function *getStdLibFunction(const std::string &name);
        // --- End Added for Standard Library ---

        /**
         * @brief Creates an Alloca instruction in the entry block of a function.
         * Used for allocating stack space for local variables.
         * @param theFunction The function containing the variable.
         * @param varName The name of the variable (for IR readability).
         * @param varType The LLVM type of the variable.
         * @return Pointer to the created AllocaInst.
         */
        llvm::AllocaInst *createEntryBlockAlloca(llvm::Function *theFunction,
                                                 const std::string &varName,
                                                 llvm::Type *varType);

        /**
         * @brief Converts a Tocin::Type to its corresponding llvm::Type.
         * Needs to be fully implemented to handle all Tocin types (primitives, classes, etc.).
         * @param tocinType Shared pointer to the Tocin::Type.
         * @return Pointer to the corresponding llvm::Type or nullptr if unsupported.
         */
        llvm::Type *getLLVMType(std::shared_ptr<Tocin::Type> tocinType);

    public:
        /**
         * @brief Constructs the IRGenerator.
         * @param ctx The LLVM context.
         * @param mod The LLVM module to populate.
         * @param eh The error handler for reporting errors.
         * @param tc The type checker for querying type information.
         */
        IRGenerator(llvm::LLVMContext &ctx, llvm::Module &mod, error::ErrorHandler &eh, TypeChecker &tc)
            : context(ctx), builder(ctx), module(mod), errorHandler(eh), typeChecker(tc)
        {
            // Declare standard library functions as soon as the generator is created
            declareStandardLibraryFunctions();
        }

        /**
         * @brief Generates LLVM IR for a list of statements (e.g., the main program).
         * @param statements Vector of statement AST nodes.
         */
        void generate(const std::vector<std::unique_ptr<Stmt>> &statements);

        // --- Visitor Methods for AST Nodes ---
        // Implementations for these will go in ir_generator.cpp

        // Statements
        void visitBlockStmt(BlockStmt *stmt) override;
        void visitExpressionStmt(ExpressionStmt *stmt) override;
        void visitFunctionStmt(FunctionStmt *stmt) override; // Generates function definition
        void visitIfStmt(IfStmt *stmt) override;
        void visitReturnStmt(ReturnStmt *stmt) override;
        void visitVariableDecl(VariableDecl *stmt) override; // Handles 'let'/'var'
        void visitWhileStmt(WhileStmt *stmt) override;
        // Add other statement visitors: ForStmt, ClassStmt, ImportStmt, etc.

        // Expressions
        void visitAssignmentExpr(AssignmentExpr *expr) override;
        void visitBinaryExpr(BinaryExpr *expr) override;
        void visitCallExpr(CallExpr *expr) override; // Needs implementation for stdlib & user funcs
        void visitGroupingExpr(GroupingExpr *expr) override;
        void visitLiteralExpr(LiteralExpr *expr) override;
        void visitLogicalExpr(LogicalExpr *expr) override;
        void visitUnaryExpr(UnaryExpr *expr) override;
        void visitVariableExpr(VariableExpr *expr) override; // Loads value from variable
        // Add other expression visitors: GetExpr, SetExpr, ListExpr, DictExpr, LambdaExpr, AwaitExpr etc.

        // Default visitor (optional, for catching unhandled nodes)
        // void visitNode(ASTNode* node) override { /* Report error or ignore */ }
    };

} // namespace codegen

#endif // IR_GENERATOR_H
