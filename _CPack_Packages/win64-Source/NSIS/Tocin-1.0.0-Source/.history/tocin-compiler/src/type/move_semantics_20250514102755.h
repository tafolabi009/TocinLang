#pragma once

#include "../pch.h"
#include "../ast/ast.h"
#include "../error/error_handler.h"
#include "ownership.h"

namespace type_checker
{

    /**
     * @brief Enum for different move scenarios
     */
    enum class MoveKind
    {
        EXPLICIT_MOVE, // User explicitly called move()
        AUTO_MOVE,     // Compiler generated move (return value, etc.)
        MOVE_ASSIGN,   // Move assignment (x = std::move(y))
        MOVE_CONSTRUCT // Move construction (X x = std::move(y))
    };

    /**
     * @brief AST node for a move expression
     *
     * Represents an explicit move operation, similar to std::move in C++.
     */
    class MoveExpr : public ast::Expr
    {
    public:
        ast::ExprPtr expr; // The expression being moved

        MoveExpr(ast::ExprPtr expr)
            : expr(expr) {}

        virtual void accept(ast::Visitor &visitor) override
        {
            // This would be implemented when we add the visitor method
            // visitor.visitMoveExpr(this);
        }

        virtual ast::TypePtr getType() const override
        {
            // Same type as the moved expression
            return expr->getType();
        }
    };

    /**
     * @brief Class managing move semantics
     *
     * Provides tools for analyzing, validating, and transforming moves.
     */
    class MoveChecker
    {
    public:
        MoveChecker(error::ErrorHandler &errorHandler, OwnershipChecker &ownershipChecker)
            : errorHandler(errorHandler), ownershipChecker(ownershipChecker) {}

        /**
         * @brief Check if a type is movable
         *
         * @param type The type to check
         * @return true if the type supports move semantics
         */
        bool isTypeMovable(ast::TypePtr type)
        {
            // By default, most types are movable
            // Only certain types like atomics or synchronization primitives might not be

            // In practice, would check type properties
            return true;
        }

        /**
         * @brief Register a new move operation
         *
         * @param source The expression being moved
         * @param destination The destination (variable name, etc.)
         * @param kind The kind of move operation
         * @return true if the move is valid
         */
        bool registerMove(ast::ExprPtr source, const std::string &destination, MoveKind kind)
        {
            // Validate that the source can be moved
            if (!canBeMovedFrom(source))
            {
                std::string errMsg = "Cannot move from this expression";
                if (auto varExpr = std::dynamic_pointer_cast<ast::VariableExpr>(source))
                {
                    errMsg = "Cannot move from variable '" + varExpr->name + "'";
                }

                errorHandler.reportError(
                    error::ErrorCode::B001_USE_AFTER_MOVE,
                    errMsg,
                    "", 0, 0, error::ErrorSeverity::ERROR);
                return false;
            }

            // Register the move in the ownership checker
            if (auto varExpr = std::dynamic_pointer_cast<ast::VariableExpr>(source))
            {
                return ownershipChecker.moveVariable(varExpr->name, destination);
            }

            // For non-variable expressions, just return true (temporaries are always movable)
            return true;
        }

        /**
         * @brief Check if an expression can be moved from
         *
         * @param expr The expression to check
         * @return true if the expression can be moved
         */
        bool canBeMovedFrom(ast::ExprPtr expr)
        {
            // Validate that the expression has a movable type
            if (!isTypeMovable(expr->getType()))
            {
                return false;
            }

            // For variables, check ownership status
            if (auto varExpr = std::dynamic_pointer_cast<ast::VariableExpr>(expr))
            {
                // Check if variable exists and hasn't been moved
                return ownershipChecker.canUseVariable(varExpr->name);
            }

            // Temporary expressions can always be moved
            return true;
        }

        /**
         * @brief Add move semantics to a function parameter
         *
         * @param parameter The function parameter
         * @param body The function body
         * @return true if successful
         */
        bool addMoveSemantics(ast::Parameter &parameter, ast::StmtPtr body)
        {
            // Check if the parameter type is movable
            if (!isTypeMovable(parameter.type))
            {
                return false;
            }

            // Mark the parameter as moved into the function
            parameter.isMoved = true;

            // In practice, would add ownership transfer in function body

            return true;
        }

        /**
         * @brief Generate a move constructor for a class
         *
         * @param className The class name
         * @param fields List of fields to move
         * @return ast::FunctionStmt* The generated move constructor
         */
        ast::FunctionStmt *generateMoveConstructor(const std::string &className,
                                                   const std::vector<ast::VarStmt *> &fields)
        {
            // Create parameters for move constructor
            ast::Parameter otherParam;
            otherParam.name = "other";
            otherParam.type = std::make_shared<ast::ClassType>(className);
            std::vector<ast::Parameter> params = {otherParam};

            // Create move constructor body
            // For each field, add move assignment
            std::vector<ast::StmtPtr> bodyStmts;

            for (ast::VarStmt *field : fields)
            {
                // Only move fields with movable types
                if (isTypeMovable(field->type))
                {
                    // Create expression: this.field = move(other.field)
                    // In practice, would construct AST nodes
                }
                else
                {
                    // For non-movable types, use copy
                    // Create expression: this.field = other.field
                }
            }

            // Combine statements into a block
            ast::StmtPtr body = std::make_shared<ast::BlockStmt>(bodyStmts);

            // Create move constructor function
            return new ast::FunctionStmt(
                className, // Constructor name matches class
                params,
                std::make_shared<ast::BasicType>(ast::TypeKind::VOID),
                body);
        }

        /**
         * @brief Generate a move assignment operator for a class
         *
         * @param className The class name
         * @param fields List of fields to move
         * @return ast::FunctionStmt* The generated move assignment operator
         */
        ast::FunctionStmt *generateMoveAssignmentOperator(const std::string &className,
                                                          const std::vector<ast::VarStmt *> &fields)
        {
            // Similar to move constructor but returns *this
            // Would implement similarly to move constructor

            // Create parameters for move assignment
            ast::Parameter otherParam;
            otherParam.name = "other";
            otherParam.type = std::make_shared<ast::ClassType>(className);
            std::vector<ast::Parameter> params = {otherParam};

            // Create move assignment body
            std::vector<ast::StmtPtr> bodyStmts;

            // Add self-assignment check
            // if (this == &other) return *this;

            // For each field, add move assignment
            for (ast::VarStmt *field : fields)
            {
                // Only move fields with movable types
                // Similar to move constructor
            }

            // Return *this
            // Add return statement

            // Combine statements into a block
            ast::StmtPtr body = std::make_shared<ast::BlockStmt>(bodyStmts);

            // Create move assignment operator
            return new ast::FunctionStmt(
                "operator=", // Operator name
                params,
                std::make_shared<ast::ClassType>(className), // Return type is the class
                body);
        }

    private:
        error::ErrorHandler &errorHandler;
        OwnershipChecker &ownershipChecker;
    };

    /**
     * @brief Utility class for handling rvalue references
     *
     * Similar to C++ rvalue references (Type&&) for move semantics.
     */
    class RValueReference
    {
    public:
        /**
         * @brief Create an rvalue reference type from a base type
         *
         * @param baseType The base type
         * @return ast::TypePtr The rvalue reference type
         */
        static ast::TypePtr createRValueRefType(ast::TypePtr baseType)
        {
            // In practice, would create a specialized type with && suffix
            return std::make_shared<ast::RValueRefType>(baseType);
        }

        /**
         * @brief Check if a type is an rvalue reference
         *
         * @param type The type to check
         * @return true if the type is an rvalue reference
         */
        static bool isRValueRefType(ast::TypePtr type)
        {
            return std::dynamic_pointer_cast<ast::RValueRefType>(type) != nullptr;
        }

        /**
         * @brief Get the base type from an rvalue reference
         *
         * @param refType The rvalue reference type
         * @return ast::TypePtr The base type
         */
        static ast::TypePtr getBaseType(ast::TypePtr refType)
        {
            if (auto rvalueRef = std::dynamic_pointer_cast<ast::RValueRefType>(refType))
            {
                return rvalueRef->baseType;
            }
            return refType; // Not an rvalue reference
        }
    };

    /**
     * @brief AST node for an rvalue reference type
     *
     * Represents a C++-style rvalue reference (Type&&).
     */
    class RValueRefType : public ast::Type
    {
    public:
        ast::TypePtr baseType; // The referenced type

        RValueRefType(ast::TypePtr baseType)
            : baseType(baseType) {}

        virtual std::string toString() const override
        {
            return baseType->toString() + "&&";
        }
    };

} // namespace type_checker
