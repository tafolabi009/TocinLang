#ifndef TOKEN_H
#define TOKEN_H

#include <iostream>
#include <string>
#include <unordered_map>

// Define the token types.
enum class TokenType {
    // Keywords
    DEF,
    CLASS,
    IF,
    ELIF,
    ELSE,
    FOR,
    IN,
    WHILE,
    RETURN,
    IMPORT,
    FROM,
    MATCH,
    CASE,
    DEFAULT,
    ASYNC,
    AWAIT,
    CONST,
    LET,
    UNSAFE,
    INTERFACE,
    OVERRIDE,
    SPAWN,
    PURE,
    TRUE,
    FALSE,
    NIL,

    // Types
    TYPE,
    INT,
    INT8,
    INT16,
    INT32,
    INT64,
    UINT,
    UINT8,
    UINT16,
    UINT32,
    UINT64,
    FLOAT32,
    FLOAT64,
    BOOL,
    CHAR,
    STRING,
    LIST,
    MAP,
    SET,
    TUPLE,
    OPTION,
    RESULT,

    // Literals
    IDENTIFIER,
    INTEGER_LITERAL,
    FLOAT_LITERAL,
    STRING_LITERAL,
    BOOL_LITERAL,

    // Operators
    PLUS,           // +
    MINUS,          // -
    STAR,           // *
    SLASH,          // /
    PERCENT,        // %
    EQUAL,          // =
    PLUS_EQUAL,     // +=
    MINUS_EQUAL,    // -=
    STAR_EQUAL,     // *=
    SLASH_EQUAL,    // /=
    PERCENT_EQUAL,  // %=
    BANG,           // !
    BANG_EQUAL,     // !=
    EQUAL_EQUAL,    // ==
    GREATER,        // >
    GREATER_EQUAL,  // >=
    LESS,           // <
    LESS_EQUAL,     // <=
    AND,            // and
    OR,             // or
    ARROW,          // ->
    COLON,          // :
    DOUBLE_COLON,   // ::
    DOT,            // .
    COMMA,          // ,
    QUESTION,       // ?
    AT,             // @
    HASH,           // #

    // Delimiters
    LEFT_PAREN,     // (
    RIGHT_PAREN,    // )
    LEFT_BRACKET,   // [
    RIGHT_BRACKET,  // ]
    LEFT_BRACE,     // {
    RIGHT_BRACE,    // }

    // Indentation
    INDENT,
    DEDENT,
    NEWLINE,

    // Special
    EOF_TOKEN,
    ERROR
};

// Token structure representing a single token.
struct Token {
    TokenType type;
    std::string value;
    std::string filename;
    size_t line;
    size_t column;

    Token(TokenType type,
        const std::string& value,
        const std::string& filename,
        size_t line,
        size_t column)
        : type(type), value(value), filename(filename), line(line), column(column) {}

    // Default constructor to create an empty token.
    Token()
        : type(TokenType::ERROR),
        value(""),
        filename(""),
        line(0),
        column(0) {}

    // Returns a string representation of the token.
    std::string toString() const;
};

// Mapping table from TokenType to fully qualified name strings.
static const std::unordered_map<TokenType, std::string> tokenTypeToString = {
    {TokenType::DEF, "TokenType::DEF"},
    {TokenType::CLASS, "TokenType::CLASS"},
    {TokenType::IF, "TokenType::IF"},
    {TokenType::ELIF, "TokenType::ELIF"},
    {TokenType::ELSE, "TokenType::ELSE"},
    {TokenType::FOR, "TokenType::FOR"},
    {TokenType::IN, "TokenType::IN"},
    {TokenType::WHILE, "TokenType::WHILE"},
    {TokenType::RETURN, "TokenType::RETURN"},
    {TokenType::IMPORT, "TokenType::IMPORT"},
    {TokenType::FROM, "TokenType::FROM"},
    {TokenType::MATCH, "TokenType::MATCH"},
    {TokenType::CASE, "TokenType::CASE"},
    {TokenType::DEFAULT, "TokenType::DEFAULT"},
    {TokenType::ASYNC, "TokenType::ASYNC"},
    {TokenType::AWAIT, "TokenType::AWAIT"},
    {TokenType::CONST, "TokenType::CONST"},
    {TokenType::LET, "TokenType::LET"},
    {TokenType::UNSAFE, "TokenType::UNSAFE"},
    {TokenType::INTERFACE, "TokenType::INTERFACE"},
    {TokenType::OVERRIDE, "TokenType::OVERRIDE"},
    {TokenType::SPAWN, "TokenType::SPAWN"},
    {TokenType::PURE, "TokenType::PURE"},
    {TokenType::TRUE, "TokenType::TRUE"},
    {TokenType::FALSE, "TokenType::FALSE"},
    {TokenType::NIL, "TokenType::NIL"},

    {TokenType::TYPE, "TokenType::TYPE"},
    {TokenType::INT, "TokenType::INT"},
    {TokenType::INT8, "TokenType::INT8"},
    {TokenType::INT16, "TokenType::INT16"},
    {TokenType::INT32, "TokenType::INT32"},
    {TokenType::INT64, "TokenType::INT64"},
    {TokenType::UINT, "TokenType::UINT"},
    {TokenType::UINT8, "TokenType::UINT8"},
    {TokenType::UINT16, "TokenType::UINT16"},
    {TokenType::UINT32, "TokenType::UINT32"},
    {TokenType::UINT64, "TokenType::UINT64"},
    {TokenType::FLOAT32, "TokenType::FLOAT32"},
    {TokenType::FLOAT64, "TokenType::FLOAT64"},
    {TokenType::BOOL, "TokenType::BOOL"},
    {TokenType::CHAR, "TokenType::CHAR"},
    {TokenType::STRING, "TokenType::STRING"},
    {TokenType::LIST, "TokenType::LIST"},
    {TokenType::MAP, "TokenType::MAP"},
    {TokenType::SET, "TokenType::SET"},
    {TokenType::TUPLE, "TokenType::TUPLE"},
    {TokenType::OPTION, "TokenType::OPTION"},
    {TokenType::RESULT, "TokenType::RESULT"},

    {TokenType::IDENTIFIER, "TokenType::IDENTIFIER"},
    {TokenType::INTEGER_LITERAL, "TokenType::INTEGER_LITERAL"},
    {TokenType::FLOAT_LITERAL, "TokenType::FLOAT_LITERAL"},
    {TokenType::STRING_LITERAL, "TokenType::STRING_LITERAL"},
    {TokenType::BOOL_LITERAL, "TokenType::BOOL_LITERAL"},

    {TokenType::PLUS, "TokenType::PLUS"},
    {TokenType::MINUS, "TokenType::MINUS"},
    {TokenType::STAR, "TokenType::STAR"},
    {TokenType::SLASH, "TokenType::SLASH"},
    {TokenType::PERCENT, "TokenType::PERCENT"},
    {TokenType::EQUAL, "TokenType::EQUAL"},
    {TokenType::PLUS_EQUAL, "TokenType::PLUS_EQUAL"},
    {TokenType::MINUS_EQUAL, "TokenType::MINUS_EQUAL"},
    {TokenType::STAR_EQUAL, "TokenType::STAR_EQUAL"},
    {TokenType::SLASH_EQUAL, "TokenType::SLASH_EQUAL"},
    {TokenType::PERCENT_EQUAL, "TokenType::PERCENT_EQUAL"},
    {TokenType::BANG, "TokenType::BANG"},
    {TokenType::BANG_EQUAL, "TokenType::BANG_EQUAL"},
    {TokenType::EQUAL_EQUAL, "TokenType::EQUAL_EQUAL"},
    {TokenType::GREATER, "TokenType::GREATER"},
    {TokenType::GREATER_EQUAL, "TokenType::GREATER_EQUAL"},
    {TokenType::LESS, "TokenType::LESS"},
    {TokenType::LESS_EQUAL, "TokenType::LESS_EQUAL"},
    {TokenType::AND, "TokenType::AND"},
    {TokenType::OR, "TokenType::OR"},
    {TokenType::ARROW, "TokenType::ARROW"},
    {TokenType::COLON, "TokenType::COLON"},
    {TokenType::DOUBLE_COLON, "TokenType::DOUBLE_COLON"},
    {TokenType::DOT, "TokenType::DOT"},
    {TokenType::COMMA, "TokenType::COMMA"},
    {TokenType::QUESTION, "TokenType::QUESTION"},
    {TokenType::AT, "TokenType::AT"},
    {TokenType::HASH, "TokenType::HASH"},
    {TokenType::SEMICOLON}

    {TokenType::LEFT_PAREN, "TokenType::LEFT_PAREN"},
    {TokenType::RIGHT_PAREN, "TokenType::RIGHT_PAREN"},
    {TokenType::LEFT_BRACKET, "TokenType::LEFT_BRACKET"},
    {TokenType::RIGHT_BRACKET, "TokenType::RIGHT_BRACKET"},
    {TokenType::LEFT_BRACE, "TokenType::LEFT_BRACE"},
    {TokenType::RIGHT_BRACE, "TokenType::RIGHT_BRACE"},

    {TokenType::INDENT, "TokenType::INDENT"},
    {TokenType::DEDENT, "TokenType::DEDENT"},
    {TokenType::NEWLINE, "TokenType::NEWLINE"},

    {TokenType::EOF_TOKEN, "TokenType::EOF_TOKEN"},
    {TokenType::ERROR, "TokenType::ERROR"}
};

#endif // TOKEN_H
