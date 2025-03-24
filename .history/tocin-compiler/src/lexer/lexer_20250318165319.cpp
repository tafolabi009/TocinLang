

// tocin-compiler/src/lexer/lexer.cpp
#include "token.h"
#include "lexer.h"
#include <stdexcept>
#include <cctype>
#include <sstream>

std::unordered_map<std::string, TokenType> Lexer::keywords = {
    {"def", TokenType::DEF},
    {"class", TokenType::CLASS},
    {"if", TokenType::IF},
    {"elif", TokenType::ELIF},
    {"else", TokenType::ELSE},
    {"for", TokenType::FOR},
    {"in", TokenType::IN},
    {"while", TokenType::WHILE},
    {"return", TokenType::RETURN},
    {"import", TokenType::IMPORT},
    {"from", TokenType::FROM},
    {"match", TokenType::MATCH},
    {"case", TokenType::CASE},
    {"default", TokenType::DEFAULT},
    {"async", TokenType::ASYNC},
    {"await", TokenType::AWAIT},
    {"const", TokenType::CONST},
    {"let", TokenType::LET},
    {"unsafe", TokenType::UNSAFE},
    {"interface", TokenType::INTERFACE},
    {"override", TokenType::OVERRIDE},
    {"spawn", TokenType::SPAWN},
    {"pure", TokenType::PURE},
    {"type", TokenType::TYPE},
    {"int", TokenType::INT},
    {"int8", TokenType::INT8},
    {"int16", TokenType::INT16},
    {"int32", TokenType::INT32},
    {"int64", TokenType::INT64},
    {"uint", TokenType::UINT},
    {"uint8", TokenType::UINT8},
    {"uint16", TokenType::UINT16},
    {"uint32", TokenType::UINT32},
    {"uint64", TokenType::UINT64},
    {"float32", TokenType::FLOAT32},
    {"float64", TokenType::FLOAT64},
    {"bool", TokenType::BOOL},
    {"char", TokenType::CHAR},
    {"string", TokenType::STRING},
    {"list", TokenType::LIST},
    {"map", TokenType::MAP},
    {"set", TokenType::SET},
    {"tuple", TokenType::TUPLE},
    {"Option", TokenType::OPTION},
    {"Result", TokenType::RESULT},
    {"True", TokenType::BOOL_LITERAL},
    {"False", TokenType::BOOL_LITERAL},
    {"None", TokenType::BOOL_LITERAL},
    {"print", TokenType::PR}};

Lexer::Lexer(const std::string& source, const std::string& filename)
    : source(source), filename(filename) {}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (position < source.length()) {
        if (atLineStart) {
            handleIndentation();
            atLineStart = false;
        }

        char c = peek();

        if (isspace(c)) {
            if (c == '\n') {
                tokens.push_back(makeToken(TokenType::NEWLINE));
                advance();
                atLineStart = true;
            }
            else {
                skipWhitespace();
            }
        }
        else if (isalpha(c) || c == '_') {
            tokens.push_back(scanIdentifier());
        }
        else if (isdigit(c)) {
            tokens.push_back(scanNumber());
        }
        else if (c == '"' || c == '\'') {
            tokens.push_back(scanString());
        }
        else if (c == '#') {
            // Skip comments
            while (peek() != '\n' && position < source.length()) {
                advance();
            }
        }
        else {
            // Handle operators and symbols
            switch (c) {
            case '(': tokens.push_back(makeToken(TokenType::LEFT_PAREN, "(")); advance(); break;
            case ')': tokens.push_back(makeToken(TokenType::RIGHT_PAREN, ")")); advance(); break;
            case '[': tokens.push_back(makeToken(TokenType::LEFT_BRACKET, "[")); advance(); break;
            case ']': tokens.push_back(makeToken(TokenType::RIGHT_BRACKET, "]")); advance(); break;
            case '{': tokens.push_back(makeToken(TokenType::LEFT_BRACE, "{")); advance(); break;
            case '}': tokens.push_back(makeToken(TokenType::RIGHT_BRACE, "}")); advance(); break;
            case '+':
                if (match('=')) {
                    tokens.push_back(makeToken(TokenType::PLUS_EQUAL, "+="));
                }
                else {
                    tokens.push_back(makeToken(TokenType::PLUS, "+"));
                }
                break;
            case '-':
                if (match('=')) {
                    tokens.push_back(makeToken(TokenType::MINUS_EQUAL, "-="));
                }
                else if (match('>')) {
                    tokens.push_back(makeToken(TokenType::ARROW, "->"));
                }
                else {
                    tokens.push_back(makeToken(TokenType::MINUS, "-"));
                }
                break;
            case '*':
                if (match('=')) {
                    tokens.push_back(makeToken(TokenType::STAR_EQUAL, "*="));
                }
                else {
                    tokens.push_back(makeToken(TokenType::STAR, "*"));
                }
                break;
            case '/':
                if (match('=')) {
                    tokens.push_back(makeToken(TokenType::SLASH_EQUAL, "/="));
                }
                else {
                    tokens.push_back(makeToken(TokenType::SLASH, "/"));
                }
                break;
            case '%':
                if (match('=')) {
                    tokens.push_back(makeToken(TokenType::PERCENT_EQUAL, "%="));
                }
                else {
                    tokens.push_back(makeToken(TokenType::PERCENT, "%"));
                }
                break;
            case '=':
                if (match('=')) {
                    tokens.push_back(makeToken(TokenType::EQUAL_EQUAL, "=="));
                }
                else {
                    tokens.push_back(makeToken(TokenType::EQUAL, "="));
                }
                break;
            case '!':
                if (match('=')) {
                    tokens.push_back(makeToken(TokenType::BANG_EQUAL, "!="));
                }
                else {
                    tokens.push_back(makeToken(TokenType::BANG, "!"));
                }
                break;
            case '>':
                if (match('=')) {
                    tokens.push_back(makeToken(TokenType::GREATER_EQUAL, ">="));
                }
                else {
                    tokens.push_back(makeToken(TokenType::GREATER, ">"));
                }
                break;
            case '<':
                if (match('=')) {
                    tokens.push_back(makeToken(TokenType::LESS_EQUAL, "<="));
                }
                else {
                    tokens.push_back(makeToken(TokenType::LESS, "<"));
                }
                break;
            case ':':
                if (match(':')) {
                    tokens.push_back(makeToken(TokenType::DOUBLE_COLON, "::"));
                }
                else {
                    tokens.push_back(makeToken(TokenType::COLON, ":"));
                }
                break;
            case '.': tokens.push_back(makeToken(TokenType::DOT, ".")); advance(); break;
            case ',': tokens.push_back(makeToken(TokenType::COMMA, ",")); advance(); break;
            case '?': tokens.push_back(makeToken(TokenType::QUESTION, "?")); advance(); break;
            case '@': tokens.push_back(makeToken(TokenType::AT, "@")); advance(); break;
            default:
                std::stringstream ss;
                ss << "Unexpected character '" << c << "' at " << filename << ":" << line << ":" << column;
                tokens.push_back(makeToken(TokenType::ERROR, ss.str()));
                advance();
            }
        }
    }

    // Handle any remaining indentation
    while (indentLevel > 0) {
        tokens.push_back(makeToken(TokenType::DEDENT));
        indentLevel--;
    }

    tokens.push_back(makeToken(TokenType::EOF_TOKEN));

    return tokens;
}

char Lexer::peek() const {
    if (position >= source.length()) {
        return '\0';
    }
    return source[position];
}

char Lexer::advance() {
    char c = peek();
    position++;
    column++;

    if (c == '\n') {
        line++;
        column = 1;
    }

    return c;
}

bool Lexer::match(char expected) {
    if (peek() != expected) {
        return false;
    }
    advance();
    return true;
}

void Lexer::skipWhitespace() {
    while (isspace(peek()) && peek() != '\n') {
        advance();
    }
}

void Lexer::handleIndentation() {
    int spaces = 0;

    while (peek() == ' ' || peek() == '\t') {
        if (peek() == ' ') {
            spaces++;
        }
        else {
            spaces += 4; // Treat tabs as 4 spaces
        }
        advance();
    }

    // If this is just a blank line or a comment, ignore indentation
    if (peek() == '\n' || peek() == '#') {
        return;
    }

    int newIndentLevel = spaces / 4; // Using 4 spaces as indentation level

    // Generate INDENT or DEDENT tokens as needed
    if (newIndentLevel > indentLevel) {
        for (int i = indentLevel; i < newIndentLevel; i++) {
            makeToken(TokenType::INDENT);
        }
    }
    else if (newIndentLevel < indentLevel) {
        for (int i = indentLevel; i > newIndentLevel; i--) {
            makeToken(TokenType::DEDENT);
        }
    }

    indentLevel = newIndentLevel;
}

Token Lexer::makeToken(TokenType type, const std::string& value) {
    return Token(type, value, filename, line, column);
}

Token Lexer::scanIdentifier() {
    size_t start = position;

    while (isalnum(peek()) || peek() == '_') {
        advance();
    }

    std::string text = source.substr(start, position - start);

    // Check if this is a keyword
    auto it = keywords.find(text);
    if (it != keywords.end()) {
        return makeToken(it->second, text);
    }

    return makeToken(TokenType::IDENTIFIER, text);
}

Token Lexer::scanNumber() {
    size_t start = position;
    bool isFloat = false;

    while (isdigit(peek())) {
        advance();
    }

    // Look for a decimal point
    if (peek() == '.' && isdigit(source[position + 1])) {
        isFloat = true;
        advance(); // Consume the '.'

        while (isdigit(peek())) {
            advance();
        }
    }

    std::string number = source.substr(start, position - start);

    if (isFloat) {
        return makeToken(TokenType::FLOAT_LITERAL, number);
    }
    else {
        return makeToken(TokenType::INTEGER_LITERAL, number);
    }
}

Token Lexer::scanString() {
    char quoteType = peek();
    advance(); // Consume the opening quote

    size_t start = position;

    while (peek() != quoteType && peek() != '\0') {
        if (peek() == '\\') {
            advance(); // Skip the escape character
        }
        advance();
    }

    if (peek() == '\0') {
        return makeToken(TokenType::ERROR, "Unterminated string");
    }

    std::string value = source.substr(start, position - start);
    advance(); // Consume the closing quote

    return makeToken(TokenType::STRING_LITERAL, value);
}
