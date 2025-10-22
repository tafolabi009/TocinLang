#ifndef PARSER_H
#define PARSER_H

#include "../ast/ast.h"
#include "../lexer/lexer.h"
#include "../error/error_handler.h"
#include <memory>
#include <vector>
#include <algorithm>
#include <string>
#include <utility>

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

        /**
         * @brief Improved error recovery context
         */
        struct ErrorContext {
            std::string message;
            lexer::Token token;
            std::vector<lexer::TokenType> expectedTokens;
            bool isFatal;
            
            ErrorContext() : isFatal(false) {}
        };
        
        /**
         * @brief Get all parsing errors encountered
         */
        const std::vector<ErrorContext>& getErrors() const { return errors_; }
        
        /**
         * @brief Check if parser has encountered fatal errors
         */
        bool hasFatalErrors() const {
            return std::any_of(errors_.begin(), errors_.end(),
                             [](const ErrorContext& e) { return e.isFatal; });
        }

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
        ast::StmtPtr goStmt();
        ast::StmtPtr selectStmt();
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
        ast::ExprPtr channelSendExpr();
        ast::ExprPtr channelReceiveExpr();
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
        std::vector<ErrorContext> errors_;
        
        // Enhanced error recovery
        void recordError(const std::string& message, const lexer::Token& token,
                        const std::vector<lexer::TokenType>& expected = {},
                        bool isFatal = false);
        bool synchronizeToToken(lexer::TokenType type);
        bool synchronizeToAny(const std::vector<lexer::TokenType>& types);
        void skipUntilSynchronizationPoint();
        
        // Operator precedence parsing
        ast::ExprPtr parseBinaryExpression(int minPrecedence);
        int getOperatorPrecedence(lexer::TokenType type) const;
        bool isRightAssociative(lexer::TokenType type) const;
        
        // Enhanced validation
        bool validateExpression(ast::ExprPtr expr);
        bool validateStatement(ast::StmtPtr stmt);
        bool validateType(ast::TypePtr type);
    };

} // namespace parser

#endif // PARSER_H
