#ifndef LEXER_H
#define LEXER_H

#include "token.h"
#include "../error/error_handler.h"
#include <string>
#include <vector>
#include <memory>

namespace lexer {

    /**
     * @brief Lexer class for tokenizing Tocin source code.
     */
    class Lexer {
    public:
        /**
         * @brief Constructs a lexer.
         * @param source The source code to tokenize.
         * @param filename The source file name.
         * @param indentSize The number of spaces per indentation level (default 4).
         */
        Lexer(const std::string& source, const std::string& filename, int indentSize = 4);

        /**
         * @brief Tokenizes the source code.
         * @return A vector of tokens.
         */
        std::vector<Token> tokenize();

    private:
        bool isAtEnd() const;
        char advance();
        char peek() const;
        char peekNext() const;
        bool match(char expected);
        void skipWhitespace();
        void handleIndentation();
        void scanToken();
        void scanString();
        void scanNumber();
        void scanIdentifier();
        void scanTemplateLiteral();
        void reportError(error::ErrorCode code, const std::string& message);
        Token makeToken(TokenType type, const std::string& value = "");

        std::string source;
        std::string filename;
        std::vector<Token> tokens;
        size_t start;
        size_t current;
        int line;
        int column;
        int indentLevel;
        bool atLineStart;
        int indentSize;
        error::ErrorHandler errorHandler;
        int errorCount;
        int maxErrors;
    };

} // namespace lexer

#endif // LEXER_H