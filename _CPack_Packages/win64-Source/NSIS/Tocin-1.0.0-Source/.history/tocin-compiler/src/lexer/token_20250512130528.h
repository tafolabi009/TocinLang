#ifndef TOKEN_H
#define TOKEN_H

#include <string>
#include <string_view>

/**
 * @brief Namespace for lexer-related functionality.
 */
namespace lexer
{

    /**
     * @brief Enum representing token types in the Tocin language.
     */
    enum class TokenType
    {
        // Single-character tokens
        LEFT_PAREN,
        RIGHT_PAREN,
        LEFT_BRACE,
        RIGHT_BRACE,
        LEFT_BRACKET,
        RIGHT_BRACKET,
        COMMA,
        DOT,
        SEMI_COLON,
        COLON,

        // One or two character tokens
        PLUS,
        PLUS_EQUAL,
        MINUS,
        MINUS_EQUAL,
        STAR,
        STAR_EQUAL,
        SLASH,
        SLASH_EQUAL,
        PERCENT,
        PERCENT_EQUAL,
        EQUAL,
        EQUAL_EQUAL,
        BANG,
        BANG_EQUAL,
        LESS,
        LESS_EQUAL,
        GREATER,
        GREATER_EQUAL,
        DOUBLE_COLON,
        ARROW,

        // Logical operators
        AND,
        OR,

        // Literals
        IDENTIFIER,
        STRING,
        INT,
        FLOAT64,
        FLOAT32,

        // Keywords
        LET,
        DEF,
        ASYNC,
        AWAIT,
        CLASS,
        IF,
        ELIF,
        ELSE,
        WHILE,
        FOR,
        IN,
        RETURN,
        IMPORT,
        FROM,
        MATCH,
        CASE,
        DEFAULT,
        CONST,
        TRUE,
        FALSE,
        NIL,
        LAMBDA,
        PRINT,
        NEW,
        DELETE,

        // Indentation
        INDENT,
        DEDENT,

        // Special tokens
        ERROR,
        EOF_TOKEN
    };

    /**
     * @brief Class representing a single token in the source code.
     */
    class Token
    {
    public:
        /**
         * @brief Constructs a token.
         * @param type The type of the token.
         * @param value The lexeme or value of the token.
         * @param filename The source file name (interned).
         * @param line The line number.
         * @param column The column number.
         */
        Token(TokenType type, std::string value, std::string_view filename, int line, int column)
            : type(type), value(std::move(value)), filename(filename), line(line), column(column) {}

        /**
         * @brief Converts the token to a string for debugging.
         * @return A string representation of the token.
         */
        std::string toString() const;

        TokenType type;
        std::string value;
        std::string_view filename;
        int line;
        int column;
    };

} // namespace lexer

#endif // TOKEN_H
