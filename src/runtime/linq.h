#pragma once

#include "../pch.h"
#include "../ast/ast.h"
#include "../error/error_handler.h"

namespace runtime
{

    /**
     * @brief Enumeration of supported LINQ-style operations
     *
     * These operations can be applied to collections (list, array, etc.)
     * to perform various transformations and queries.
     */
    enum class LinqOperation
    {
        WHERE,    // Filter elements based on a predicate
        SELECT,   // Transform each element
        ORDER_BY, // Sort elements by key
        GROUP_BY, // Group elements by key
        JOIN,     // Join two collections
        TAKE,     // Take first N elements
        SKIP,     // Skip first N elements
        FIRST,    // Get first element
        LAST,     // Get last element
        COUNT,    // Count elements
        SUM,      // Sum numeric elements
        AVERAGE,  // Average numeric elements
        ANY,      // Check if any element satisfies a condition
        ALL,      // Check if all elements satisfy a condition
        MIN,      // Find the minimum element
        MAX       // Find the maximum element
    };

    /**
     * @brief Base class for LINQ-style query expressions
     *
     * Represents a query operation that can be applied to a collection.
     */
    class LinqExpr : public ast::Expression
    {
    public:
        ast::ExprPtr source;     // The source collection
        LinqOperation operation; // The operation to apply
        ast::ExprPtr predicate;  // Optional predicate/selector function

        LinqExpr(ast::ExprPtr source, LinqOperation operation, ast::ExprPtr predicate = nullptr)
            : ast::Expression(lexer::Token()), source(source), operation(operation), predicate(predicate) {}

        virtual void accept(ast::Visitor &visitor) override
        {
            // This would be implemented when we add the visitor method
            // visitor.visitLinqExpr(this);
        }

        virtual ast::TypePtr getType() const override
        {
            // Type depends on the operation and source collection
            // This is just a placeholder
            return nullptr;
        }
    };

    /**
     * @brief Extended LINQ expression that supports method chaining
     *
     * This expression type allows for fluent syntax like:
     * collection.where(x => x > 5).select(x => x * 2).orderBy(x => x)
     */
    class ChainedLinqExpr : public LinqExpr
    {
    public:
        std::shared_ptr<LinqExpr> previous; // Previous operation in the chain

        ChainedLinqExpr(std::shared_ptr<LinqExpr> previous,
                        LinqOperation operation,
                        ast::ExprPtr predicate = nullptr)
            : LinqExpr(previous->source, operation, predicate), previous(previous) {}

        virtual void accept(ast::Visitor &visitor) override
        {
            // This would be implemented when we add the visitor method
            // visitor.visitChainedLinqExpr(this);
        }
    };

    /**
     * @brief Helper class for type checking and analyzing LINQ expressions
     */
    class LinqAnalyzer
    {
    public:
        LinqAnalyzer(error::ErrorHandler &errorHandler)
            : errorHandler(errorHandler) {}

        /**
         * @brief Check if a type can be used with LINQ operations
         *
         * @param type The type to check
         * @return true if the type is a valid collection for LINQ
         */
        bool isValidLinqSource(ast::TypePtr type)
        {
            // Check if it's a collection type
            // This is a simplified implementation - would need more logic in practice
            if (auto genericType = std::dynamic_pointer_cast<ast::GenericType>(type))
            {
                return genericType->name == "list" ||
                       genericType->name == "array" ||
                       genericType->name == "vector";
            }
            return false;
        }

        /**
         * @brief Get the element type from a collection type
         *
         * @param collectionType The collection type
         * @return ast::TypePtr The element type
         */
        ast::TypePtr getElementType(ast::TypePtr collectionType)
        {
            if (auto genericType = std::dynamic_pointer_cast<ast::GenericType>(collectionType))
            {
                if (!genericType->typeArguments.empty())
                {
                    return genericType->typeArguments[0];
                }
            }
            return nullptr;
        }

        /**
         * @brief Check if a predicate is valid for the given operation and collection
         *
         * @param operation The LINQ operation
         * @param collectionType The collection type
         * @param predicateType The predicate function type
         * @return true if the predicate is valid
         */
        bool validatePredicate(LinqOperation operation, ast::TypePtr collectionType, ast::TypePtr predicateType)
        {
            // Get the element type from the collection
            ast::TypePtr elementType = getElementType(collectionType);
            if (!elementType)
            {
                errorHandler.reportError(
                    error::ErrorCode::T031_UNDEFINED_TYPE,
                    "Element type '" + elementType->toString() + "' is not defined",
                    "", 0, 0, error::ErrorSeverity::ERROR);
                return false;
            }

            // Validate based on operation
            // This is simplified - would need more detailed type checking in practice
            switch (operation)
            {
            case LinqOperation::WHERE:
                // Predicate should be (T) -> bool
                // Simplified check
                return true;

            case LinqOperation::SELECT:
                // Selector can transform to any type
                return true;

            case LinqOperation::ORDER_BY:
                // Key selector should return a comparable type
                return true;

                // Would implement more cases...

            default:
                return true;
            }
        }

        /**
         * @brief Determine the result type of a LINQ operation
         *
         * @param operation The LINQ operation
         * @param sourceType The source collection type
         * @param predicateType The predicate/selector function type
         * @return ast::TypePtr The result type
         */
        ast::TypePtr getResultType(LinqOperation operation, ast::TypePtr sourceType, ast::TypePtr predicateType)
        {
            ast::TypePtr elementType = getElementType(sourceType);

            switch (operation)
            {
            case LinqOperation::WHERE:
                // Same collection type with same element type
                return sourceType;

            case LinqOperation::SELECT:
                // Same collection type with transformed element type
                // Would need to determine the return type of the selector
                // This is simplified
                return std::make_shared<ast::GenericType>(lexer::Token(), "list", std::vector<ast::TypePtr>{elementType});

            case LinqOperation::FIRST:
            case LinqOperation::LAST:
                // Return the element type
                return elementType;

            case LinqOperation::COUNT:
                // Returns an integer
                return std::make_shared<ast::BasicType>(ast::TypeKind::INT);

                // Would implement more cases...

            default:
                // Default to source type
                return sourceType;
            }
        }

    private:
        error::ErrorHandler &errorHandler;
    };

    /**
     * @brief Extension methods for collections to support LINQ-style operations
     *
     * These would be implemented as part of the standard library.
     */
    struct LinqExtensionMethods
    {
        /**
         * @brief Registers all LINQ extension methods
         *
         * This would be called during compiler initialization to register
         * extension methods for collections.
         */
        static void registerExtensionMethods()
        {
            // Implementation would register methods like:
            // list<T>.where(predicate: (T) -> bool): list<T>
            // list<T>.select<R>(selector: (T) -> R): list<R>
            // etc.
        }
    };

} // namespace runtime
