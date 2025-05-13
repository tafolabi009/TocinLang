#pragma once

#include "../lexer/token.h"
#include <memory>
#include <string>
#include <vector>

namespace ast {

// Forward declaration for Visitor
class Visitor;

/**
 * @brief Base class for all statement nodes in the AST.
 */
class Statement {
public:
    explicit Statement(const lexer::Token &token) : token(token) {}
    virtual ~Statement() = default;
    
    /**
     * @brief Accept a visitor for processing this statement.
     * @param visitor The visitor to accept.
     */
    virtual void accept(Visitor &visitor) = 0;
    
    /**
     * @brief Get the token associated with this statement.
     * @return The token.
     */
    const lexer::Token &getToken() const { return token; }

protected:
    lexer::Token token;
};

// Define shared_ptr type for statements
using StmtPtr = std::shared_ptr<Statement>;

} // namespace ast 
