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

        // Enhanced operators
        INCREMENT,
        DECREMENT,
        POWER,
        POWER_EQUAL,
        STRICT_EQUAL,
        STRICT_NOT_EQUAL,
        LEFT_SHIFT,
        LEFT_SHIFT_EQUAL,
        RIGHT_SHIFT,
        RIGHT_SHIFT_EQUAL,
        BITWISE_AND,
        BITWISE_AND_EQUAL,
        BITWISE_OR,
        BITWISE_OR_EQUAL,
        BITWISE_XOR,
        BITWISE_XOR_EQUAL,
        BITWISE_NOT,
        SAFE_ACCESS,
        NULL_COALESCE,
        QUESTION,
        ELLIPSIS,
        RANGE,

        // Template literals
        TEMPLATE_START,
        TEMPLATE_EXPR,
        TEMPLATE_END,

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

        // Enhanced keywords
        TRY,
        CATCH,
        FINALLY,
        THROW,
        BREAK,
        CONTINUE,
        SWITCH,
        ENUM,
        STRUCT,
        INTERFACE,
        TRAIT,
        IMPL,
        PUB,
        PRIV,
        STATIC,
        FINAL,
        ABSTRACT,
        VIRTUAL,
        OVERRIDE,
        SUPER,
        SELF,
        NULL_TOKEN,
        UNDEFINED,
        VOID,
        TYPEOF,
        INSTANCEOF,
        AS,
        IS,
        WHERE,
        YIELD,
        GENERATOR,
        COROUTINE,
        CHANNEL,
        CHANNEL_SEND,
        CHANNEL_RECEIVE,
        SELECT,
        SPAWN,
        GO,
        JOIN,
        MUTEX,
        LOCK,
        UNLOCK,
        ATOMIC,
        VOLATILE,
        CONSTEXPR,
        INLINE,
        EXTERN,
        EXPORT,
        MODULE,
        PACKAGE,
        NAMESPACE,
        USING,
        WITH,
        DEFER,
        PANIC,
        RECOVER,
        ASSERT,
        DEBUG,
        TRACE,
        LOG,
        WARN,
        ERROR_TOKEN,
        FATAL,

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
         * @brief Default constructor for Token.
         * Creates an "unknown" token at position 0:0.
         */
        Token()
            : type(TokenType::ERROR), value(""), filename(""), line(0), column(0) {}

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
