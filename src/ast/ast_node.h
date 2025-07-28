#pragma once

#include "../lexer/token.h"
#include <memory>

namespace tocin {
namespace ast {

/**
 * @brief Base class for all AST nodes in the Tocin language
 */
class ASTNode {
public:
    explicit ASTNode(const lexer::Token& token) : token_(token) {}
    virtual ~ASTNode() = default;

    const lexer::Token& getToken() const { return token_; }
    
    // Virtual accept method for visitor pattern
    virtual void accept(class ASTVisitor& visitor) = 0;

protected:
    lexer::Token token_;
};

/**
 * @brief Base class for all program nodes (top-level constructs)
 */
class Program : public ASTNode {
public:
    explicit Program(const lexer::Token& token) : ASTNode(token) {}
    virtual ~Program() = default;
    
    void accept(class ASTVisitor& visitor) override;
};

} // namespace ast
} // namespace tocin