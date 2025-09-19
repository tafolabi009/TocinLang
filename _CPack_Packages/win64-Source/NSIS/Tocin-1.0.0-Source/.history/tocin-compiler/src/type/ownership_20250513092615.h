#pragma once

#include "../pch.h"
#include "../ast/ast.h"
#include "../error/error_handler.h"

namespace type_checker
{

    /**
     * @brief Enumerates different ownership types in the memory model
     *
     * Follows Rust's ownership model:
     * - OWNED: Value is owned by the current scope
     * - BORROWED: Value is borrowed temporarily (immutable reference)
     * - MUTABLE_BORROWED: Value is borrowed with mutation rights
     * - MOVED: Value has been moved and original is invalidated
     */
    enum class OwnershipType
    {
        OWNED,            // Value is owned by the current scope
        BORROWED,         // Value is borrowed temporarily (immutable reference)
        MUTABLE_BORROWED, // Value is borrowed with mutation rights (mutable reference)
        MOVED             // Value has been moved and original is invalidated
    };

    /**
     * @brief Tracks the ownership status of variables
     */
    struct OwnershipInfo
    {
        OwnershipType type;                 // Current ownership type
        int borrowCount;                    // Number of active immutable borrows
        bool mutablyBorrowed;               // Whether it's currently mutably borrowed
        std::string currentOwner;           // ID of the current owner (scope or variable)
        std::vector<std::string> borrowers; // List of current borrowers

        OwnershipInfo()
            : type(OwnershipType::OWNED),
              borrowCount(0),
              mutablyBorrowed(false) {}
    };

    /**
     * @brief Manages variable ownership and borrowing in the compiler
     *
     * This class implements Rust-like ownership semantics:
     * 1. Each value has exactly one owner at a time
     * 2. Ownership can be transferred (moved)
     * 3. Values can be borrowed, either mutably (one borrower) or immutably (multiple borrowers)
     * 4. References must always be valid (no dangling pointers)
     */
    class OwnershipChecker
    {
    public:
        OwnershipChecker(error::ErrorHandler &errorHandler)
            : errorHandler(errorHandler) {}

        /**
         * @brief Enter a new scope
         */
        void enterScope()
        {
            scopes.push_back({}); // Add a new scope
        }

        /**
         * @brief Exit the current scope
         *
         * Drops all variables owned by this scope
         */
        void exitScope()
        {
            if (scopes.empty())
                return;

            // Any borrowed variables in this scope need to be returned
            auto &currentScope = scopes.back();
            for (const auto &[name, info] : currentScope)
            {
                if (info.type == OwnershipType::BORROWED ||
                    info.type == OwnershipType::MUTABLE_BORROWED)
                {
                    // Return the borrowed value to its owner
                    returnBorrowed(name);
                }

                // For moved variables, no special action needed
                // For owned variables, they're dropped naturally
            }

            scopes.pop_back();
        }

        /**
         * @brief Declare a new variable with ownership
         *
         * @param name Variable name
         * @param isMutable Whether the variable can be modified
         * @return true if declaration is valid, false if error
         */
        bool declareVariable(const std::string &name, bool isMutable)
        {
            if (scopes.empty())
            {
                scopes.push_back({}); // Create a global scope if none exists
            }

            // Check if variable already exists in the current scope
            if (variableExistsInCurrentScope(name))
            {
                errorHandler.reportError(
                    error::ErrorCode::T002_UNDEFINED_VARIABLE,
                    "Variable '" + name + "' already declared in this scope",
                    "", 0, 0, error::ErrorSeverity::ERROR);
                return false;
            }

            // Create ownership info for the new variable
            OwnershipInfo info;
            info.type = OwnershipType::OWNED;
            info.currentOwner = getCurrentScopeId();

            // Add to current scope
            scopes.back()[name] = info;

            // Track mutability
            if (isMutable)
            {
                mutableVariables.insert(name);
            }

            return true;
        }

        /**
         * @brief Check if a variable exists in any visible scope
         */
        bool variableExists(const std::string &name)
        {
            // Search from innermost to outermost scope
            for (auto it = scopes.rbegin(); it != scopes.rend(); ++it)
            {
                if (it->find(name) != it->end())
                {
                    return true;
                }
            }
            return false;
        }

        /**
         * @brief Get the ownership status of a variable
         */
        OwnershipInfo getOwnershipInfo(const std::string &name)
        {
            // Search from innermost to outermost scope
            for (auto it = scopes.rbegin(); it != scopes.rend(); ++it)
            {
                auto findIt = it->find(name);
                if (findIt != it->end())
                {
                    return findIt->second;
                }
            }

            // Not found - return default
            return OwnershipInfo();
        }

        /**
         * @brief Borrow a variable (immutably)
         *
         * @param name Variable to borrow
         * @param borrower Identifier for the borrower
         * @return true if borrowing is allowed
         */
        bool borrowVariable(const std::string &name, const std::string &borrower)
        {
            // Find the variable
            auto &info = findVariableInfo(name);
            if (info.type == OwnershipType::MOVED)
            {
                errorHandler.reportError(
                    error::ErrorCode::B001_USE_AFTER_MOVE,
                    "Cannot borrow '" + name + "' - value has been moved",
                    "", 0, 0, error::ErrorSeverity::ERROR);
                return false;
            }

            // Cannot borrow if mutably borrowed already
            if (info.mutablyBorrowed)
            {
                errorHandler.reportError(
                    error::ErrorCode::B002_BORROW_CONFLICT,
                    "Cannot borrow '" + name + "' as immutable - already borrowed as mutable",
                    "", 0, 0, error::ErrorSeverity::ERROR);
                return false;
            }

            // Update borrowing info
            info.borrowCount++;
            info.borrowers.push_back(borrower);

            return true;
        }

        /**
         * @brief Borrow a variable mutably
         *
         * @param name Variable to borrow
         * @param borrower Identifier for the borrower
         * @return true if mutable borrowing is allowed
         */
        bool borrowMutVariable(const std::string &name, const std::string &borrower)
        {
            // Find the variable
            auto &info = findVariableInfo(name);

            // Check variable is mutable
            if (!isVariableMutable(name))
            {
                errorHandler.reportError(
                    error::ErrorCode::B003_MUTABILITY_ERROR,
                    "Cannot borrow '" + name + "' as mutable - variable is not declared as mutable",
                    "", 0, 0, error::ErrorSeverity::ERROR);
                return false;
            }

            if (info.type == OwnershipType::MOVED)
            {
                errorHandler.reportError(
                    error::ErrorCode::B001_USE_AFTER_MOVE,
                    "Cannot borrow '" + name + "' - value has been moved",
                    "", 0, 0, error::ErrorSeverity::ERROR);
                return false;
            }

            // Cannot mutably borrow if any borrows exist
            if (info.borrowCount > 0 || info.mutablyBorrowed)
            {
                errorHandler.reportError(
                    error::ErrorCode::B002_BORROW_CONFLICT,
                    "Cannot borrow '" + name + "' as mutable - already borrowed",
                    "", 0, 0, error::ErrorSeverity::ERROR);
                return false;
            }

            // Update borrowing info
            info.mutablyBorrowed = true;
            info.borrowers.push_back(borrower);

            return true;
        }

        /**
         * @brief Move a value from one variable to another
         *
         * @param source Source variable name
         * @param dest Destination variable name
         * @return true if move is allowed
         */
        bool moveVariable(const std::string &source, const std::string &dest)
        {
            // Get source info
            auto &sourceInfo = findVariableInfo(source);

            // Cannot move if borrowed
            if (sourceInfo.borrowCount > 0 || sourceInfo.mutablyBorrowed)
            {
                errorHandler.reportError(
                    error::ErrorCode::B004_MOVE_BORROWED_VALUE,
                    "Cannot move '" + source + "' - value is borrowed",
                    "", 0, 0, error::ErrorSeverity::ERROR);
                return false;
            }

            // Cannot move if already moved
            if (sourceInfo.type == OwnershipType::MOVED)
            {
                errorHandler.reportError(
                    error::ErrorCode::B001_USE_AFTER_MOVE,
                    "Cannot move '" + source + "' - value has already been moved",
                    "", 0, 0, error::ErrorSeverity::ERROR);
                return false;
            }

            // Mark source as moved
            sourceInfo.type = OwnershipType::MOVED;

            // If destination is in the current scope, update its ownership info
            if (variableExistsInCurrentScope(dest))
            {
                auto &destInfo = findVariableInfo(dest);
                destInfo.type = OwnershipType::OWNED;
                destInfo.currentOwner = getCurrentScopeId();
            }

            return true;
        }

        /**
         * @brief Return a borrowed variable
         *
         * @param name Variable name
         * @param borrower Identifier of the borrower returning the value
         * @return true if return is valid
         */
        bool returnBorrowed(const std::string &name, const std::string &borrower = "")
        {
            auto &info = findVariableInfo(name);

            // Remove this borrower
            if (!borrower.empty())
            {
                auto it = std::find(info.borrowers.begin(), info.borrowers.end(), borrower);
                if (it != info.borrowers.end())
                {
                    info.borrowers.erase(it);
                }
            }
            else
            {
                // If no specific borrower, clear all from current scope
                info.borrowers.clear();
            }

            // Update borrowing status
            if (info.mutablyBorrowed)
            {
                info.mutablyBorrowed = false;
            }
            else if (info.borrowCount > 0)
            {
                info.borrowCount--;
            }

            return true;
        }

        /**
         * @brief Check if a variable can be used (not moved)
         */
        bool canUseVariable(const std::string &name)
        {
            auto info = getOwnershipInfo(name);
            return info.type != OwnershipType::MOVED;
        }

        /**
         * @brief Check if a variable can be modified
         */
        bool canModifyVariable(const std::string &name)
        {
            // Must be mutable and not borrowed immutably
            if (!isVariableMutable(name))
                return false;

            auto info = getOwnershipInfo(name);
            if (info.type == OwnershipType::MOVED)
                return false;
            if (info.borrowCount > 0)
                return false;

            return true;
        }

    private:
        error::ErrorHandler &errorHandler;

        // Stack of scopes, each with variables and their ownership info
        std::vector<std::map<std::string, OwnershipInfo>> scopes;

        // Set of variables that are declared as mutable
        std::set<std::string> mutableVariables;

        // Current scope ID counter for tracking ownership
        int currentScopeId = 0;

        /**
         * @brief Check if variable exists in current scope
         */
        bool variableExistsInCurrentScope(const std::string &name)
        {
            if (scopes.empty())
                return false;
            return scopes.back().find(name) != scopes.back().end();
        }

        /**
         * @brief Get a unique identifier for the current scope
         */
        std::string getCurrentScopeId()
        {
            return "scope_" + std::to_string(currentScopeId++);
        }

        /**
         * @brief Find a variable's ownership info
         */
        OwnershipInfo &findVariableInfo(const std::string &name)
        {
            // Search from innermost to outermost scope
            for (auto it = scopes.rbegin(); it != scopes.rend(); ++it)
            {
                auto findIt = it->find(name);
                if (findIt != it->end())
                {
                    return findIt->second;
                }
            }

            // If not found, add to current scope (should not happen in practice)
            OwnershipInfo info;
            scopes.back()[name] = info;
            return scopes.back()[name];
        }

        /**
         * @brief Check if a variable is declared as mutable
         */
        bool isVariableMutable(const std::string &name)
        {
            return mutableVariables.find(name) != mutableVariables.end();
        }
    };

} // namespace type_checker
