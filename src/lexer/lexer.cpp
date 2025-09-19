#include "lexer.h"
#include <cctype>
#include <stdexcept>
#include <unordered_map>
#include <algorithm>

namespace lexer
{

    static const std::unordered_map<std::string, TokenType> keywords = {
        {"let", TokenType::LET}, {"def", TokenType::DEF}, {"async", TokenType::ASYNC}, 
        {"await", TokenType::AWAIT}, {"class", TokenType::CLASS}, {"if", TokenType::IF}, 
        {"elif", TokenType::ELIF}, {"else", TokenType::ELSE}, {"while", TokenType::WHILE}, 
        {"for", TokenType::FOR}, {"in", TokenType::IN}, {"return", TokenType::RETURN}, 
        {"import", TokenType::IMPORT}, {"from", TokenType::FROM}, {"match", TokenType::MATCH}, 
        {"case", TokenType::CASE}, {"default", TokenType::DEFAULT}, {"const", TokenType::CONST}, 
        {"true", TokenType::TRUE}, {"false", TokenType::FALSE}, {"None", TokenType::NIL}, 
        {"and", TokenType::AND}, {"or", TokenType::OR}, {"lambda", TokenType::LAMBDA}, 
        {"print", TokenType::PRINT}, {"new", TokenType::NEW}, {"delete", TokenType::DELETE},
        {"try", TokenType::TRY}, {"catch", TokenType::CATCH}, {"finally", TokenType::FINALLY},
        {"throw", TokenType::THROW}, {"break", TokenType::BREAK}, {"continue", TokenType::CONTINUE},
        {"switch", TokenType::SWITCH}, {"enum", TokenType::ENUM}, {"struct", TokenType::STRUCT},
        {"interface", TokenType::INTERFACE}, {"trait", TokenType::TRAIT}, {"impl", TokenType::IMPL},
        {"pub", TokenType::PUB}, {"priv", TokenType::PRIV}, {"static", TokenType::STATIC},
        {"final", TokenType::FINAL}, {"abstract", TokenType::ABSTRACT}, {"virtual", TokenType::VIRTUAL},
        {"override", TokenType::OVERRIDE}, {"super", TokenType::SUPER}, {"self", TokenType::SELF},
        {"null", TokenType::NULL_TOKEN}, {"undefined", TokenType::UNDEFINED}, {"void", TokenType::VOID},
        {"typeof", TokenType::TYPEOF}, {"instanceof", TokenType::INSTANCEOF}, {"as", TokenType::AS},
        {"is", TokenType::IS}, {"where", TokenType::WHERE}, {"yield", TokenType::YIELD},
        {"generator", TokenType::GENERATOR}, {"coroutine", TokenType::COROUTINE}, {"channel", TokenType::CHANNEL},
        {"select", TokenType::SELECT}, {"spawn", TokenType::SPAWN}, {"go", TokenType::GO}, {"join", TokenType::JOIN},
        {"mutex", TokenType::MUTEX}, {"lock", TokenType::LOCK}, {"unlock", TokenType::UNLOCK},
        {"atomic", TokenType::ATOMIC}, {"volatile", TokenType::VOLATILE}, {"constexpr", TokenType::CONSTEXPR},
        {"inline", TokenType::INLINE}, {"extern", TokenType::EXTERN}, {"export", TokenType::EXPORT},
        {"module", TokenType::MODULE}, {"package", TokenType::PACKAGE}, {"namespace", TokenType::NAMESPACE},
        {"using", TokenType::USING}, {"with", TokenType::WITH}, {"defer", TokenType::DEFER},
        {"panic", TokenType::PANIC}, {"recover", TokenType::RECOVER}, {"assert", TokenType::ASSERT},
        {"debug", TokenType::DEBUG}, {"trace", TokenType::TRACE}, {"log", TokenType::LOG},
        {"warn", TokenType::WARN}, {"error", TokenType::ERROR}, {"fatal", TokenType::FATAL}
    };

    Lexer::Lexer(const std::string &source, const std::string &filename, int indentSize)
        : source(source), filename(filename), start(0), current(0), line(1), column(1),
          indentLevel(0), atLineStart(true), indentSize(indentSize), errorCount(0), maxErrors(100) {}

    std::vector<Token> Lexer::tokenize()
    {
        tokens.clear();
        start = 0;
        current = 0;
        line = 1;
        column = 1;
        indentLevel = 0;
        atLineStart = true;
        errorCount = 0;

        while (!isAtEnd() && errorCount < maxErrors)
        {
            start = current;
            try
            {
                scanToken();
            }
            catch (const std::runtime_error &e)
            {
                reportError(error::ErrorCode::L001_INVALID_CHARACTER, e.what());
                advance(); // Skip the problematic character
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
        if (isAtEnd()) return '\0';
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
                // Don't break here - continue to handle indentation on next line
                continue;
            }
            else if (c == '#')
            {
                // Handle both single-line and multi-line comments
                advance(); // consume '#'
                if (match('#')) // ## for multi-line comments
                {
                    while (!isAtEnd() && !(peek() == '#' && peekNext() == '#'))
                    {
                        if (peek() == '\n')
                        {
                            ++line;
                            column = 1;
                            atLineStart = true;
                        }
                        advance();
                    }
                    if (match('#')) advance(); // consume closing ##
                    // Continue the loop to handle any subsequent whitespace/comments
                    continue;
                }
                else // Single-line comment
                {
                    while (!isAtEnd() && peek() != '\n')
                        advance();
                    // If we hit a newline, advance past it and set atLineStart
                    if (!isAtEnd() && peek() == '\n')
                    {
                        ++line;
                        column = 1;
                        atLineStart = true;
                        advance();
                        // Continue the loop to handle any subsequent whitespace/comments
                        continue;
                    }
                }
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
            reportError(error::ErrorCode::L001_INVALID_CHARACTER, 
                       "Mixed tabs and spaces in indentation");
            return;
        }

        if (peek() == '\n' || isAtEnd())
        {
            return;
        }
        
        if (peek() == '#')
        {
            // Handle comments at the start of a line
            advance(); // consume '#'
            if (match('#')) // ## for multi-line comments
            {
                while (!isAtEnd() && !(peek() == '#' && peekNext() == '#'))
                {
                    if (peek() == '\n')
                    {
                        ++line;
                        column = 1;
                        atLineStart = true;
                    }
                    advance();
                }
                if (match('#')) advance(); // consume closing ##
            }
            else // Single-line comment
            {
                while (!isAtEnd() && peek() != '\n')
                    advance();
                // If we hit a newline, advance past it and set atLineStart
                if (!isAtEnd() && peek() == '\n')
                {
                    ++line;
                    column = 1;
                    atLineStart = true;
                    advance();
                }
            }
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
        char quote = advance(); // consume the opening quote
        bool escaped = false;
        std::string value;
        int startLine = line;
        int startColumn = column - 1; // Adjust for the quote character

        while (!isAtEnd())
        {
            char c = peek();
            
            if (c == '\n')
            {
                reportError(error::ErrorCode::L002_UNTERMINATED_STRING,
                           "Unterminated string literal");
                tokens.emplace_back(TokenType::ERROR, "", filename, startLine, startColumn);
                return;
            }

            if (escaped)
            {
                switch (c)
                {
                case 'n': value += '\n'; break;
                case 'r': value += '\r'; break;
                case 't': value += '\t'; break;
                case 'b': value += '\b'; break;
                case 'f': value += '\f'; break;
                case 'v': value += '\v'; break;
                case 'a': value += '\a'; break;
                case '\\': value += '\\'; break;
                case '\'': value += '\''; break;
                case '"': value += '"'; break;
                case '0': value += '\0'; break;
                case 'x': // Hex escape
                    if (isAtEnd() || !std::isxdigit(peekNext()))
                    {
                        reportError(error::ErrorCode::L005_INVALID_ESCAPE_SEQUENCE,
                                   "Invalid hex escape sequence");
                        value += 'x';
                    }
                    else
                    {
                        advance(); // consume 'x'
                        std::string hex;
                        hex += advance(); // First hex digit
                        if (!isAtEnd() && std::isxdigit(peek()))
                        {
                            hex += advance(); // Second hex digit
                        }
                        value += (char)std::stoi(hex, nullptr, 16);
                    }
                    break;
                case 'u': // Unicode escape
                    if (isAtEnd() || peek() != '{')
                    {
                        reportError(error::ErrorCode::L006_INVALID_UNICODE_ESCAPE,
                                   "Invalid unicode escape sequence");
                        value += 'u';
                    }
                    else
                    {
                        advance(); // consume '{'
                        std::string unicode;
                        while (!isAtEnd() && peek() != '}' && std::isxdigit(peek()))
                        {
                            unicode += advance();
                        }
                        if (isAtEnd() || peek() != '}' || unicode.empty() || unicode.length() > 6)
                        {
                            reportError(error::ErrorCode::L006_INVALID_UNICODE_ESCAPE,
                                       "Invalid unicode escape sequence");
                            value += "u{" + unicode;
                        }
                        else
                        {
                            advance(); // consume '}'
                            // Convert unicode to UTF-8
                            uint32_t codepoint = std::stoul(unicode, nullptr, 16);
                            if (codepoint <= 0x7F)
                            {
                                value += (char)codepoint;
                            }
                            else if (codepoint <= 0x7FF)
                            {
                                value += (char)(0xC0 | (codepoint >> 6));
                                value += (char)(0x80 | (codepoint & 0x3F));
                            }
                            else if (codepoint <= 0xFFFF)
                            {
                                value += (char)(0xE0 | (codepoint >> 12));
                                value += (char)(0x80 | ((codepoint >> 6) & 0x3F));
                                value += (char)(0x80 | (codepoint & 0x3F));
                            }
                            else
                            {
                                value += (char)(0xF0 | (codepoint >> 18));
                                value += (char)(0x80 | ((codepoint >> 12) & 0x3F));
                                value += (char)(0x80 | ((codepoint >> 6) & 0x3F));
                                value += (char)(0x80 | (codepoint & 0x3F));
                            }
                        }
                    }
                    break;
                default:
                    reportError(error::ErrorCode::L005_INVALID_ESCAPE_SEQUENCE,
                               std::string("Invalid escape sequence: \\") + c);
                    value += c;
                    break;
                }
                escaped = false;
            }
            else if (c == '\\')
            {
                escaped = true;
            }
            else if (c == quote)
            {
                advance(); // consume closing quote
                tokens.emplace_back(TokenType::STRING, value, filename, startLine, startColumn);
                return;
            }
            else
            {
                value += c;
            }
            
            advance();
        }

        // If we get here, the string was not terminated
        reportError(error::ErrorCode::L002_UNTERMINATED_STRING,
                   "Unterminated string literal");
        tokens.emplace_back(TokenType::ERROR, "", filename, startLine, startColumn);
    }

    void Lexer::scanNumber()
    {
        std::string value;
        bool isFloat = false;
        bool isHex = false;
        bool isBinary = false;
        bool isOctal = false;
        
        // Start with the current character (which was peeked at)
        value += advance();
        
        // Check for hex, binary, or octal prefixes
        if (value == "0" && !isAtEnd()) {
            char next = peek();
            if (next == 'x' || next == 'X') {
                isHex = true;
                value += advance();
                if (!std::isxdigit(peek())) {
                    reportError(error::ErrorCode::L003_INVALID_NUMBER_FORMAT,
                               "Invalid hexadecimal number");
                    tokens.emplace_back(TokenType::ERROR, value, filename, line, column - value.length());
                    return;
                }
            } else if (next == 'b' || next == 'B') {
                isBinary = true;
                value += advance();
                if (peek() != '0' && peek() != '1') {
                    reportError(error::ErrorCode::L003_INVALID_NUMBER_FORMAT,
                               "Invalid binary number");
                    tokens.emplace_back(TokenType::ERROR, value, filename, line, column - value.length());
                    return;
                }
            } else if (std::isdigit(next) && next != '0') {
                isOctal = true;
            }
        }
        
        // Scan digits
        if (isHex) {
            while (std::isxdigit(peek())) {
                value += advance();
            }
        } else if (isBinary) {
            while (peek() == '0' || peek() == '1') {
                value += advance();
            }
        } else if (isOctal) {
            while (std::isdigit(peek()) && peek() < '8') {
                value += advance();
            }
        } else {
            while (std::isdigit(peek())) {
                value += advance();
            }
        }

        // Handle decimal point
        if (!isHex && !isBinary && !isOctal && peek() == '.' && std::isdigit(peekNext()))
        {
            isFloat = true;
            value += advance();
            while (std::isdigit(peek()))
            {
                value += advance();
            }
        }

        // Handle exponent
        if (!isHex && !isBinary && !isOctal && (peek() == 'e' || peek() == 'E'))
        {
            isFloat = true;
            value += advance();
            if (peek() == '+' || peek() == '-') {
                value += advance();
            }
            if (!std::isdigit(peek())) {
                reportError(error::ErrorCode::L003_INVALID_NUMBER_FORMAT,
                           "Invalid exponent in number");
                tokens.emplace_back(TokenType::ERROR, value, filename, line, column - value.length());
                return;
            }
            while (std::isdigit(peek())) {
                value += advance();
            }
        }

        // Handle type suffixes
        if (!isAtEnd()) {
            char suffix = peek();
            if (suffix == 'f' || suffix == 'F') {
                isFloat = true;
                value += advance();
            } else if (suffix == 'l' || suffix == 'L') {
                value += advance();
            } else if (suffix == 'u' || suffix == 'U') {
                value += advance();
            }
        }

        if (isFloat) {
            tokens.emplace_back(TokenType::FLOAT64, value, filename, line, column - value.length());
        } else {
            tokens.emplace_back(TokenType::INT, value, filename, line, column - value.length());
        }
    }

    void Lexer::scanIdentifier()
    {
        std::string value;
        // Start with the current character (which was peeked at)
        value += advance();
        // Continue scanning the rest of the identifier
        while (std::isalnum(peek()) || peek() == '_')
        {
            value += advance();
        }
        
        // Check for keywords
        TokenType type = TokenType::IDENTIFIER;
        auto it = keywords.find(value);
        if (it != keywords.end()) {
            type = it->second;
        }
        
        tokens.emplace_back(type, value, filename, line, column - value.length());
    }

    void Lexer::scanToken()
{
    if (atLineStart) {
        handleIndentation();
        atLineStart = false;
        // Skip any remaining whitespace after indentation
        while (peek() == ' ' || peek() == '\r' || peek() == '\t') {
            advance();
        }
    } else {
        skipWhitespace();
    }
    start = current;
        if (isAtEnd())
            return;

        char c = peek();  // Don't advance yet, just peek
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

        // For all other tokens, advance and process
        advance();  // Now advance for non-identifier tokens

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
            if (match('.')) {
                if (match('.')) {
                    tokens.emplace_back(TokenType::ELLIPSIS, "...", filename, line, column - 3);
                } else {
                    tokens.emplace_back(TokenType::RANGE, "..", filename, line, column - 2);
                }
            } else {
                tokens.emplace_back(TokenType::DOT, ".", filename, line, column - 1);
            }
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
            else if (match('+'))
            {
                tokens.emplace_back(TokenType::INCREMENT, "++", filename, line, column - 2);
            }
            else
            {
                tokens.emplace_back(TokenType::PLUS, "+", filename, line, column - 1);
            }
            break;
        case '-':
            if (match('<'))
            {
                tokens.emplace_back(TokenType::CHANNEL_RECEIVE, "-<", filename, line, column - 2);
            }
            else if (match('='))
            {
                tokens.emplace_back(TokenType::MINUS_EQUAL, "-=", filename, line, column - 2);
            }
            else if (match('-'))
            {
                tokens.emplace_back(TokenType::DECREMENT, "--", filename, line, column - 2);
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
            else if (match('*'))
            {
                if (match('=')) {
                    tokens.emplace_back(TokenType::POWER_EQUAL, "**=", filename, line, column - 3);
                } else {
                    tokens.emplace_back(TokenType::POWER, "**", filename, line, column - 2);
                }
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
                if (match('=')) {
                    tokens.emplace_back(TokenType::STRICT_EQUAL, "===", filename, line, column - 3);
                } else {
                    tokens.emplace_back(TokenType::EQUAL_EQUAL, "==", filename, line, column - 2);
                }
            }
            else
            {
                tokens.emplace_back(TokenType::EQUAL, "=", filename, line, column - 1);
            }
            break;
        case '!':
            if (match('='))
            {
                if (match('=')) {
                    tokens.emplace_back(TokenType::STRICT_NOT_EQUAL, "!==", filename, line, column - 3);
                } else {
                    tokens.emplace_back(TokenType::BANG_EQUAL, "!=", filename, line, column - 2);
                }
            }
            else
            {
                tokens.emplace_back(TokenType::BANG, "!", filename, line, column - 1);
            }
            break;
        case '<':
            if (match('-'))
            {
                tokens.emplace_back(TokenType::CHANNEL_SEND, "<-", filename, line, column - 2);
            }
            else if (match('='))
            {
                tokens.emplace_back(TokenType::LESS_EQUAL, "<=", filename, line, column - 2);
            }
            else if (match('<'))
            {
                if (match('=')) {
                    tokens.emplace_back(TokenType::LEFT_SHIFT_EQUAL, "<<=", filename, line, column - 3);
                } else {
                    tokens.emplace_back(TokenType::LEFT_SHIFT, "<<", filename, line, column - 2);
                }
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
            else if (match('>'))
            {
                if (match('=')) {
                    tokens.emplace_back(TokenType::RIGHT_SHIFT_EQUAL, ">>=", filename, line, column - 3);
                } else {
                    tokens.emplace_back(TokenType::RIGHT_SHIFT, ">>", filename, line, column - 2);
                }
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
            else if (match('='))
            {
                tokens.emplace_back(TokenType::BITWISE_AND_EQUAL, "&=", filename, line, column - 2);
            }
            else
            {
                tokens.emplace_back(TokenType::BITWISE_AND, "&", filename, line, column - 1);
            }
            break;
        case '|':
            if (match('|'))
            {
                tokens.emplace_back(TokenType::OR, "||", filename, line, column - 2);
            }
            else if (match('='))
            {
                tokens.emplace_back(TokenType::BITWISE_OR_EQUAL, "|=", filename, line, column - 2);
            }
            else
            {
                tokens.emplace_back(TokenType::BITWISE_OR, "|", filename, line, column - 1);
            }
            break;
        case '^':
            if (match('='))
            {
                tokens.emplace_back(TokenType::BITWISE_XOR_EQUAL, "^=", filename, line, column - 2);
            }
            else
            {
                tokens.emplace_back(TokenType::BITWISE_XOR, "^", filename, line, column - 1);
            }
            break;
        case '~':
            tokens.emplace_back(TokenType::BITWISE_NOT, "~", filename, line, column - 1);
            break;
        case '?':
            if (match('.'))
            {
                tokens.emplace_back(TokenType::SAFE_ACCESS, "?.", filename, line, column - 2);
            }
            else if (match('?'))
            {
                tokens.emplace_back(TokenType::NULL_COALESCE, "??", filename, line, column - 2);
            }
            else
            {
                tokens.emplace_back(TokenType::QUESTION, "?", filename, line, column - 1);
            }
            break;
        case '"':
        case '\'':
            scanString();
            break;
        case '`':
            scanTemplateLiteral();
            break;
        default:
            reportError(error::ErrorCode::L001_INVALID_CHARACTER,
                       "Unexpected character: " + std::string(1, c));
            tokens.emplace_back(TokenType::ERROR, std::string(1, c), filename, line, column - 1);
            break;
        }
    }

    void Lexer::scanTemplateLiteral()
    {
        advance(); // consume the backtick
        std::string value;
        int startLine = line;
        int startColumn = column;

        while (!isAtEnd() && peek() != '`')
        {
            if (peek() == '$' && peekNext() == '{')
            {
                // Template interpolation
                advance(); // consume '$'
                advance(); // consume '{'
                tokens.emplace_back(TokenType::TEMPLATE_START, value, filename, startLine, startColumn);
                value.clear();
                
                // Parse the expression inside ${}
                int braceCount = 1;
                std::string expr;
                while (!isAtEnd() && braceCount > 0)
                {
                    char c = peek();
                    if (c == '{') braceCount++;
                    else if (c == '}') braceCount--;
                    else if (c == '\n') {
                        reportError(error::ErrorCode::L002_UNTERMINATED_STRING,
                                   "Unterminated template literal expression");
                        return;
                    }
                    if (braceCount > 0) {
                        expr += advance();
                    }
                }
                
                if (braceCount == 0) {
                    advance(); // consume closing '}'
                    tokens.emplace_back(TokenType::TEMPLATE_EXPR, expr, filename, line, column - expr.length() - 1);
                    startLine = line;
                    startColumn = column;
                }
            }
            else if (peek() == '\\')
            {
                // Handle escape sequences
                advance();
                switch (peek())
                {
                case 'n': value += '\n'; break;
                case 'r': value += '\r'; break;
                case 't': value += '\t'; break;
                case '`': value += '`'; break;
                case '$': value += '$'; break;
                case '\\': value += '\\'; break;
                default: value += '\\'; value += peek(); break;
                }
                advance();
            }
            else
            {
                if (peek() == '\n')
                {
                    ++line;
                    column = 1;
                }
                value += advance();
            }
        }

        if (isAtEnd())
        {
            reportError(error::ErrorCode::L002_UNTERMINATED_STRING,
                       "Unterminated template literal");
            return;
        }

        advance(); // consume closing backtick
        tokens.emplace_back(TokenType::TEMPLATE_END, value, filename, startLine, startColumn);
    }

    void Lexer::reportError(error::ErrorCode code, const std::string& message)
    {
        errorCount++;
        errorHandler.reportError(code, message, filename, line, column, error::ErrorSeverity::ERROR);
        
        if (errorCount >= maxErrors) {
            errorHandler.reportError(error::ErrorCode::L004_TOO_MANY_ERRORS,
                                   "Too many lexer errors, stopping tokenization",
                                   filename, line, column, error::ErrorSeverity::FATAL);
        }
    }

    Token Lexer::makeToken(TokenType type, const std::string &value)
    {
        return Token(type, value.empty() ? source.substr(start, current - start) : value,
                     filename, line, column - (value.empty() ? (current - start) : value.length()));
    }

} // namespace lexer
