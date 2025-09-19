#include "lexer.h"
#include <cctype>
#include <stdexcept>
#include <unordered_map>

namespace lexer
{

    static const std::unordered_map<std::string, TokenType> keywords = {
        {"let", TokenType::LET}, {"def", TokenType::DEF}, {"async", TokenType::ASYNC}, {"await", TokenType::AWAIT}, {"class", TokenType::CLASS}, {"if", TokenType::IF}, {"elif", TokenType::ELIF}, {"else", TokenType::ELSE}, {"while", TokenType::WHILE}, {"for", TokenType::FOR}, {"in", TokenType::IN}, {"return", TokenType::RETURN}, {"import", TokenType::IMPORT}, {"from", TokenType::FROM}, {"match", TokenType::MATCH}, {"case", TokenType::CASE}, {"default", TokenType::DEFAULT}, {"const", TokenType::CONST}, {"true", TokenType::TRUE}, {"false", TokenType::FALSE}, {"None", TokenType::NIL}, {"and", TokenType::AND}, {"or", TokenType::OR}, {"lambda", TokenType::LAMBDA}, {"print", TokenType::PRINT}};

    Lexer::Lexer(const std::string &source, const std::string &filename, int indentSize)
        : source(source), filename(filename), start(0), current(0), line(1), column(1),
          indentLevel(0), atLineStart(true), indentSize(indentSize) {}

    std::vector<Token> Lexer::tokenize()
    {
        tokens.clear();
        start = 0;
        current = 0;
        line = 1;
        column = 1;
        indentLevel = 0;
        atLineStart = true;

        while (!isAtEnd())
        {
            start = current;
            try
            {
                scanToken();
            }
            catch (const std::runtime_error &e)
            {
                errorHandler.reportError(error::ErrorCode::L001_INVALID_CHARACTER,
                                         e.what(), filename, line, column,
                                         error::ErrorSeverity::ERROR);
            }
        }

        // Add any remaining dedents
        while (indentLevel > 0)
        {
            tokens.push_back(Token(TokenType::DEDENT, "", filename, line, column));
            indentLevel--;
        }

        tokens.push_back(Token(TokenType::EOF_TOKEN, "", filename, line, column));
        return tokens;
    }

    bool Lexer::isAtEnd() const
    {
        return current >= source.length();
    }

    char Lexer::advance()
    {
        ++current;
        ++column;
        return source[current - 1];
    }

    char Lexer::peek() const
    {
        if (isAtEnd())
            return '\0';
        return source[current];
    }

    char Lexer::peekNext() const
    {
        if (current + 1 >= source.length())
            return '\0';
        return source[current + 1];
    }

    bool Lexer::match(char expected)
    {
        if (isAtEnd() || source[current] != expected)
            return false;
        ++current;
        ++column;
        return true;
    }

    void Lexer::skipWhitespace()
    {
        while (true)
        {
            char c = peek();
            if (c == ' ' || c == '\r' || c == '\t')
            {
                advance();
            }
            else if (c == '\n')
            {
                ++line;
                column = 1;
                atLineStart = true;
                advance();
                break;
            }
            else if (c == '#')
            {
                while (!isAtEnd() && peek() != '\n')
                    advance();
            }
            else
            {
                break;
            }
        }
    }

    void Lexer::handleIndentation()
    {
        int spaces = 0;
        bool usedTab = false, usedSpace = false;

        while (peek() == ' ' || peek() == '\t')
        {
            if (peek() == ' ')
            {
                spaces++;
                usedSpace = true;
            }
            else
            {
                spaces += indentSize;
                usedTab = true;
            }
            advance();
        }

        if (usedTab && usedSpace)
        {
            errorHandler.reportError("Mixed tabs and spaces in indentation", filename, line, column,
                                     error::ErrorSeverity::ERROR);
            errorHandler.setFatal(true);
            tokens.emplace_back(TokenType::ERROR, "", filename, line, column);
            return;
        }

        if (peek() == '\n' || peek() == '#' || isAtEnd())
        {
            return;
        }

        int newIndentLevel = spaces / indentSize;
        if (newIndentLevel > indentLevel)
        {
            for (int i = indentLevel; i < newIndentLevel; ++i)
            {
                tokens.emplace_back(TokenType::INDENT, "", filename, line, column);
            }
        }
        else if (newIndentLevel < indentLevel)
        {
            for (int i = indentLevel; i > newIndentLevel; --i)
            {
                tokens.emplace_back(TokenType::DEDENT, "", filename, line, column);
            }
        }
        indentLevel = newIndentLevel;
    }

    void Lexer::scanString()
    {
        bool escaped = false;
        std::string value;

        while ((!isAtEnd() && (peek() != '"' || escaped)))
        {
            if (peek() == '\n')
            {
                errorHandler.reportError(error::ErrorCode::L002_UNTERMINATED_STRING,
                                         "Unterminated string", filename, line, column,
                                         error::ErrorSeverity::ERROR);
                return;
            }

            if (escaped)
            {
                switch (peek())
                {
                case 'n':
                    value += '\n';
                    break;
                case 'r':
                    value += '\r';
                    break;
                case 't':
                    value += '\t';
                    break;
                case '"':
                    value += '"';
                    break;
                case '\\':
                    value += '\\';
                    break;
                default:
                    errorHandler.reportError(error::ErrorCode::L001_INVALID_CHARACTER,
                                             "Invalid escape sequence", filename, line, column,
                                             error::ErrorSeverity::ERROR);
                    value += peek();
                }
                escaped = false;
            }
            else if (peek() == '\\')
            {
                escaped = true;
            }
            else
            {
                value += peek();
            }

            advance();
        }

        if (isAtEnd())
        {
            errorHandler.reportError(error::ErrorCode::L002_UNTERMINATED_STRING,
                                     "Unterminated string", filename, line, column,
                                     error::ErrorSeverity::ERROR);
            return;
        }

        // Consume the closing "
        advance();

        tokens.push_back(makeToken(TokenType::STRING, value));
    }

    void Lexer::scanNumber()
    {
        bool isFloat = false;

        while (isDigit(peek()))
            advance();

        // Look for a decimal point
        if (peek() == '.' && isDigit(peekNext()))
        {
            isFloat = true;
            advance(); // Consume the '.'

            while (isDigit(peek()))
                advance();
        }

        // Look for exponent
        if ((peek() == 'e' || peek() == 'E') &&
            (isDigit(peekNext()) || (peekNext() == '+' || peekNext() == '-') && isDigit(peek())))
        {
            isFloat = true;
            advance(); // Consume the 'e' or 'E'

            if (peek() == '+' || peek() == '-')
                advance();

            if (!isDigit(peek()))
            {
                errorHandler.reportError(error::ErrorCode::L003_INVALID_NUMBER_FORMAT,
                                         "Invalid number format: missing exponent digits",
                                         filename, line, column,
                                         error::ErrorSeverity::ERROR);
                return;
            }

            while (isDigit(peek()))
                advance();
        }

        std::string value = source.substr(start, current - start);
        tokens.push_back(makeToken(isFloat ? TokenType::FLOAT64 : TokenType::INT, value));
    }

    void Lexer::scanIdentifier()
    {
        std::string value;
        while (std::isalnum(peek()) || peek() == '_')
        {
            value += advance();
        }
        TokenType type = keywords.contains(value) ? keywords.at(value) : TokenType::IDENTIFIER;
        tokens.emplace_back(type, value, filename, line, column - value.length());
    }

    void Lexer::scanToken()
    {
        skipWhitespace();
        start = current;
        if (isAtEnd())
            return;

        char c = advance();
        if (std::isalpha(c) || c == '_')
        {
            scanIdentifier();
            return;
        }
        if (std::isdigit(c))
        {
            scanNumber();
            return;
        }

        switch (c)
        {
        case '(':
            tokens.emplace_back(TokenType::LEFT_PAREN, "(", filename, line, column - 1);
            break;
        case ')':
            tokens.emplace_back(TokenType::RIGHT_PAREN, ")", filename, line, column - 1);
            break;
        case '{':
            tokens.emplace_back(TokenType::LEFT_BRACE, "{", filename, line, column - 1);
            break;
        case '}':
            tokens.emplace_back(TokenType::RIGHT_BRACE, "}", filename, line, column - 1);
            break;
        case '[':
            tokens.emplace_back(TokenType::LEFT_BRACKET, "[", filename, line, column - 1);
            break;
        case ']':
            tokens.emplace_back(TokenType::RIGHT_BRACKET, "]", filename, line, column - 1);
            break;
        case ',':
            tokens.emplace_back(TokenType::COMMA, ",", filename, line, column - 1);
            break;
        case '.':
            tokens.emplace_back(TokenType::DOT, ".", filename, line, column - 1);
            break;
        case ';':
            tokens.emplace_back(TokenType::SEMI_COLON, ";", filename, line, column - 1);
            break;
        case ':':
            if (match(':'))
            {
                tokens.emplace_back(TokenType::DOUBLE_COLON, "::", filename, line, column - 2);
            }
            else
            {
                tokens.emplace_back(TokenType::COLON, ":", filename, line, column - 1);
            }
            break;
        case '+':
            if (match('='))
            {
                tokens.emplace_back(TokenType::PLUS_EQUAL, "+=", filename, line, column - 2);
            }
            else
            {
                tokens.emplace_back(TokenType::PLUS, "+", filename, line, column - 1);
            }
            break;
        case '-':
            if (match('='))
            {
                tokens.emplace_back(TokenType::MINUS_EQUAL, "-=", filename, line, column - 2);
            }
            else if (match('>'))
            {
                tokens.emplace_back(TokenType::ARROW, "->", filename, line, column - 2);
            }
            else
            {
                tokens.emplace_back(TokenType::MINUS, "-", filename, line, column - 1);
            }
            break;
        case '*':
            if (match('='))
            {
                tokens.emplace_back(TokenType::STAR_EQUAL, "*=", filename, line, column - 2);
            }
            else
            {
                tokens.emplace_back(TokenType::STAR, "*", filename, line, column - 1);
            }
            break;
        case '/':
            if (match('='))
            {
                tokens.emplace_back(TokenType::SLASH_EQUAL, "/=", filename, line, column - 2);
            }
            else
            {
                tokens.emplace_back(TokenType::SLASH, "/", filename, line, column - 1);
            }
            break;
        case '%':
            if (match('='))
            {
                tokens.emplace_back(TokenType::PERCENT_EQUAL, "%=", filename, line, column - 2);
            }
            else
            {
                tokens.emplace_back(TokenType::PERCENT, "%", filename, line, column - 1);
            }
            break;
        case '=':
            if (match('='))
            {
                tokens.emplace_back(TokenType::EQUAL_EQUAL, "==", filename, line, column - 2);
            }
            else
            {
                tokens.emplace_back(TokenType::EQUAL, "=", filename, line, column - 1);
            }
            break;
        case '!':
            if (match('='))
            {
                tokens.emplace_back(TokenType::BANG_EQUAL, "!=", filename, line, column - 2);
            }
            else
            {
                tokens.emplace_back(TokenType::BANG, "!", filename, line, column - 1);
            }
            break;
        case '<':
            if (match('='))
            {
                tokens.emplace_back(TokenType::LESS_EQUAL, "<=", filename, line, column - 2);
            }
            else
            {
                tokens.emplace_back(TokenType::LESS, "<", filename, line, column - 1);
            }
            break;
        case '>':
            if (match('='))
            {
                tokens.emplace_back(TokenType::GREATER_EQUAL, ">=", filename, line, column - 2);
            }
            else
            {
                tokens.emplace_back(TokenType::GREATER, ">", filename, line, column - 1);
            }
            break;
        case '&':
            if (match('&'))
            {
                tokens.emplace_back(TokenType::AND, "&&", filename, line, column - 2);
            }
            else
            {
                errorHandler.reportError("Unexpected character: &", filename, line, column - 1,
                                         error::ErrorSeverity::ERROR);
                tokens.emplace_back(TokenType::ERROR, "&", filename, line, column - 1);
            }
            break;
        case '|':
            if (match('|'))
            {
                tokens.emplace_back(TokenType::OR, "||", filename, line, column - 2);
            }
            else
            {
                errorHandler.reportError("Unexpected character: |", filename, line, column - 1,
                                         error::ErrorSeverity::ERROR);
                tokens.emplace_back(TokenType::ERROR, "|", filename, line, column - 1);
            }
            break;
        case '"':
        case '\'':
            scanString();
            break;
        default:
            errorHandler.reportError("Unexpected character: " + std::string(1, c), filename, line,
                                     column - 1, error::ErrorSeverity::ERROR);
            tokens.emplace_back(TokenType::ERROR, std::string(1, c), filename, line, column - 1);
            break;
        }
    }

    Token Lexer::makeToken(TokenType type, const std::string &value)
    {
        return Token(type, value.empty() ? source.substr(start, current - start) : value,
                     filename, line, column - (value.empty() ? (current - start) : value.length()));
    }

} // namespace lexer
