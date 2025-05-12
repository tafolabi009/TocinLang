#ifndef PARSER_H
#define PARSER_H

#include "../ast/ast.h"
#include "../lexer/lexer.h"
#include "../error/error_handler.h"
#include <memory>
#include <vector>

namespace parser
{

    /**
     * @brief Parser class for constructing AST from tokens.
     */
    class Parser
    {
    public:
        /**
         * @brief Constructs a parser.
         * @param tokens The list of tokens to parse.
         */
        explicit Parser(const std::vector<lexer::Token> &tokens);

        /**
         * @brief Parses the tokens into an AST.
         * @return The root statement of the AST.
         */
        ast::StmtPtr parse();

    private:
        ast::StmtPtr declaration();
        ast::StmtPtr varDeclaration();
        ast::StmtPtr functionDeclaration();
        ast::StmtPtr classDeclaration();
        ast::StmtPtr statement();
        ast::StmtPtr expressionStmt();
        ast::StmtPtr ifStmt();
        ast::StmtPtr whileStmt();
        ast::StmtPtr forStmt();
        ast::StmtPtr blockStmt();
        ast::StmtPtr returnStmt();
        ast::StmtPtr importStmt();
        ast::StmtPtr matchStmt();
        ast::ExprPtr expression();
        ast::ExprPtr assignment();
        ast::ExprPtr orExpr();
        ast::ExprPtr andExpr();
        ast::ExprPtr equality();
        ast::ExprPtr comparison();
        ast::ExprPtr term();
        ast::ExprPtr factor();
        ast::ExprPtr unary();
        ast::ExprPtr call();
        ast::ExprPtr primary();
        ast::ExprPtr newExpr();
        ast::ExprPtr deleteExpr();
        ast::TypePtr parseType();
        std::vector<ast::Parameter> parseParameters();
        void synchronize();

        bool match(lexer::TokenType type);
        bool check(lexer::TokenType type) const;
        lexer::Token advance();
        lexer::Token peek() const;
        lexer::Token previous() const;
        bool isAtEnd() const;
        lexer::Token consume(lexer::TokenType type, const std::string &message);
        void error(const lexer::Token &token, const std::string &message);

        std::vector<lexer::Token> tokens;
        size_t current;
        error::ErrorHandler errorHandler;
    };

} // namespace parser

#endif // PARSER_H
