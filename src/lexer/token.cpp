#include "token.h"
#include <sstream>
#include <iomanip>

namespace lexer {

    std::string Token::toString() const {
        std::stringstream ss;
        std::string typeStr;
        switch (type) {
        case TokenType::LEFT_PAREN: typeStr = "LEFT_PAREN"; break;
        case TokenType::RIGHT_PAREN: typeStr = "RIGHT_PAREN"; break;
        case TokenType::LEFT_BRACE: typeStr = "LEFT_BRACE"; break;
        case TokenType::RIGHT_BRACE: typeStr = "RIGHT_BRACE"; break;
        case TokenType::LEFT_BRACKET: typeStr = "LEFT_BRACKET"; break;
        case TokenType::RIGHT_BRACKET: typeStr = "RIGHT_BRACKET"; break;
        case TokenType::COMMA: typeStr = "COMMA"; break;
        case TokenType::DOT: typeStr = "DOT"; break;
        case TokenType::SEMI_COLON: typeStr = "SEMI_COLON"; break;
        case TokenType::COLON: typeStr = "COLON"; break;
        case TokenType::PLUS: typeStr = "PLUS"; break;
        case TokenType::PLUS_EQUAL: typeStr = "PLUS_EQUAL"; break;
        case TokenType::MINUS: typeStr = "MINUS"; break;
        case TokenType::MINUS_EQUAL: typeStr = "MINUS_EQUAL"; break;
        case TokenType::STAR: typeStr = "STAR"; break;
        case TokenType::STAR_EQUAL: typeStr = "STAR_EQUAL"; break;
        case TokenType::SLASH: typeStr = "SLASH"; break;
        case TokenType::SLASH_EQUAL: typeStr = "SLASH_EQUAL"; break;
        case TokenType::PERCENT: typeStr = "PERCENT"; break;
        case TokenType::PERCENT_EQUAL: typeStr = "PERCENT_EQUAL"; break;
        case TokenType::EQUAL: typeStr = "EQUAL"; break;
        case TokenType::EQUAL_EQUAL: typeStr = "EQUAL_EQUAL"; break;
        case TokenType::BANG: typeStr = "BANG"; break;
        case TokenType::BANG_EQUAL: typeStr = "BANG_EQUAL"; break;
        case TokenType::LESS: typeStr = "LESS"; break;
        case TokenType::LESS_EQUAL: typeStr = "LESS_EQUAL"; break;
        case TokenType::GREATER: typeStr = "GREATER"; break;
        case TokenType::GREATER_EQUAL: typeStr = "GREATER_EQUAL"; break;
        case TokenType::DOUBLE_COLON: typeStr = "DOUBLE_COLON"; break;
        case TokenType::ARROW: typeStr = "ARROW"; break;
        case TokenType::INCREMENT: typeStr = "INCREMENT"; break;
        case TokenType::DECREMENT: typeStr = "DECREMENT"; break;
        case TokenType::POWER: typeStr = "POWER"; break;
        case TokenType::POWER_EQUAL: typeStr = "POWER_EQUAL"; break;
        case TokenType::STRICT_EQUAL: typeStr = "STRICT_EQUAL"; break;
        case TokenType::STRICT_NOT_EQUAL: typeStr = "STRICT_NOT_EQUAL"; break;
        case TokenType::LEFT_SHIFT: typeStr = "LEFT_SHIFT"; break;
        case TokenType::LEFT_SHIFT_EQUAL: typeStr = "LEFT_SHIFT_EQUAL"; break;
        case TokenType::RIGHT_SHIFT: typeStr = "RIGHT_SHIFT"; break;
        case TokenType::RIGHT_SHIFT_EQUAL: typeStr = "RIGHT_SHIFT_EQUAL"; break;
        case TokenType::BITWISE_AND: typeStr = "BITWISE_AND"; break;
        case TokenType::BITWISE_AND_EQUAL: typeStr = "BITWISE_AND_EQUAL"; break;
        case TokenType::BITWISE_OR: typeStr = "BITWISE_OR"; break;
        case TokenType::BITWISE_OR_EQUAL: typeStr = "BITWISE_OR_EQUAL"; break;
        case TokenType::BITWISE_XOR: typeStr = "BITWISE_XOR"; break;
        case TokenType::BITWISE_XOR_EQUAL: typeStr = "BITWISE_XOR_EQUAL"; break;
        case TokenType::BITWISE_NOT: typeStr = "BITWISE_NOT"; break;
        case TokenType::SAFE_ACCESS: typeStr = "SAFE_ACCESS"; break;
        case TokenType::NULL_COALESCE: typeStr = "NULL_COALESCE"; break;
        case TokenType::QUESTION: typeStr = "QUESTION"; break;
        case TokenType::ELLIPSIS: typeStr = "ELLIPSIS"; break;
        case TokenType::RANGE: typeStr = "RANGE"; break;
        case TokenType::TEMPLATE_START: typeStr = "TEMPLATE_START"; break;
        case TokenType::TEMPLATE_EXPR: typeStr = "TEMPLATE_EXPR"; break;
        case TokenType::TEMPLATE_END: typeStr = "TEMPLATE_END"; break;
        case TokenType::AND: typeStr = "AND"; break;
        case TokenType::OR: typeStr = "OR"; break;
        case TokenType::IDENTIFIER: typeStr = "IDENTIFIER"; break;
        case TokenType::STRING: typeStr = "STRING"; break;
        case TokenType::INT: typeStr = "INT"; break;
        case TokenType::FLOAT64: typeStr = "FLOAT64"; break;
        case TokenType::FLOAT32: typeStr = "FLOAT32"; break;
        case TokenType::LET: typeStr = "LET"; break;
        case TokenType::DEF: typeStr = "DEF"; break;
        case TokenType::ASYNC: typeStr = "ASYNC"; break;
        case TokenType::AWAIT: typeStr = "AWAIT"; break;
        case TokenType::CLASS: typeStr = "CLASS"; break;
        case TokenType::IF: typeStr = "IF"; break;
        case TokenType::ELIF: typeStr = "ELIF"; break;
        case TokenType::ELSE: typeStr = "ELSE"; break;
        case TokenType::WHILE: typeStr = "WHILE"; break;
        case TokenType::FOR: typeStr = "FOR"; break;
        case TokenType::IN: typeStr = "IN"; break;
        case TokenType::RETURN: typeStr = "RETURN"; break;
        case TokenType::IMPORT: typeStr = "IMPORT"; break;
        case TokenType::FROM: typeStr = "FROM"; break;
        case TokenType::MATCH: typeStr = "MATCH"; break;
        case TokenType::CASE: typeStr = "CASE"; break;
        case TokenType::DEFAULT: typeStr = "DEFAULT"; break;
        case TokenType::CONST: typeStr = "CONST"; break;
        case TokenType::TRUE: typeStr = "TRUE"; break;
        case TokenType::FALSE: typeStr = "FALSE"; break;
        case TokenType::NIL: typeStr = "NIL"; break;
        case TokenType::LAMBDA: typeStr = "LAMBDA"; break;
        case TokenType::PRINT: typeStr = "PRINT"; break;
        case TokenType::NEW: typeStr = "NEW"; break;
        case TokenType::DELETE: typeStr = "DELETE"; break;
        case TokenType::TRY: typeStr = "TRY"; break;
        case TokenType::CATCH: typeStr = "CATCH"; break;
        case TokenType::FINALLY: typeStr = "FINALLY"; break;
        case TokenType::THROW: typeStr = "THROW"; break;
        case TokenType::BREAK: typeStr = "BREAK"; break;
        case TokenType::CONTINUE: typeStr = "CONTINUE"; break;
        case TokenType::SWITCH: typeStr = "SWITCH"; break;
        case TokenType::ENUM: typeStr = "ENUM"; break;
        case TokenType::STRUCT: typeStr = "STRUCT"; break;
        case TokenType::INTERFACE: typeStr = "INTERFACE"; break;
        case TokenType::TRAIT: typeStr = "TRAIT"; break;
        case TokenType::IMPL: typeStr = "IMPL"; break;
        case TokenType::PUB: typeStr = "PUB"; break;
        case TokenType::PRIV: typeStr = "PRIV"; break;
        case TokenType::STATIC: typeStr = "STATIC"; break;
        case TokenType::FINAL: typeStr = "FINAL"; break;
        case TokenType::ABSTRACT: typeStr = "ABSTRACT"; break;
        case TokenType::VIRTUAL: typeStr = "VIRTUAL"; break;
        case TokenType::OVERRIDE: typeStr = "OVERRIDE"; break;
        case TokenType::SUPER: typeStr = "SUPER"; break;
        case TokenType::SELF: typeStr = "SELF"; break;
        case TokenType::NULL_TOKEN: typeStr = "NULL_TOKEN"; break;
        case TokenType::UNDEFINED: typeStr = "UNDEFINED"; break;
        case TokenType::VOID: typeStr = "VOID"; break;
        case TokenType::TYPEOF: typeStr = "TYPEOF"; break;
        case TokenType::INSTANCEOF: typeStr = "INSTANCEOF"; break;
        case TokenType::AS: typeStr = "AS"; break;
        case TokenType::IS: typeStr = "IS"; break;
        case TokenType::WHERE: typeStr = "WHERE"; break;
        case TokenType::YIELD: typeStr = "YIELD"; break;
        case TokenType::GENERATOR: typeStr = "GENERATOR"; break;
        case TokenType::COROUTINE: typeStr = "COROUTINE"; break;
        case TokenType::CHANNEL: typeStr = "CHANNEL"; break;
        case TokenType::SELECT: typeStr = "SELECT"; break;
        case TokenType::SPAWN: typeStr = "SPAWN"; break;
        case TokenType::JOIN: typeStr = "JOIN"; break;
        case TokenType::MUTEX: typeStr = "MUTEX"; break;
        case TokenType::LOCK: typeStr = "LOCK"; break;
        case TokenType::UNLOCK: typeStr = "UNLOCK"; break;
        case TokenType::ATOMIC: typeStr = "ATOMIC"; break;
        case TokenType::VOLATILE: typeStr = "VOLATILE"; break;
        case TokenType::CONSTEXPR: typeStr = "CONSTEXPR"; break;
        case TokenType::INLINE: typeStr = "INLINE"; break;
        case TokenType::EXTERN: typeStr = "EXTERN"; break;
        case TokenType::EXPORT: typeStr = "EXPORT"; break;
        case TokenType::MODULE: typeStr = "MODULE"; break;
        case TokenType::PACKAGE: typeStr = "PACKAGE"; break;
        case TokenType::NAMESPACE: typeStr = "NAMESPACE"; break;
        case TokenType::USING: typeStr = "USING"; break;
        case TokenType::WITH: typeStr = "WITH"; break;
        case TokenType::DEFER: typeStr = "DEFER"; break;
        case TokenType::PANIC: typeStr = "PANIC"; break;
        case TokenType::RECOVER: typeStr = "RECOVER"; break;
        case TokenType::ASSERT: typeStr = "ASSERT"; break;
        case TokenType::DEBUG: typeStr = "DEBUG"; break;
        case TokenType::TRACE: typeStr = "TRACE"; break;
        case TokenType::LOG: typeStr = "LOG"; break;
        case TokenType::WARN: typeStr = "WARN"; break;
        case TokenType::FATAL: typeStr = "FATAL"; break;
        case TokenType::INDENT: typeStr = "INDENT"; break;
        case TokenType::DEDENT: typeStr = "DEDENT"; break;
        case TokenType::ERROR: typeStr = "ERROR"; break;
        case TokenType::EOF_TOKEN: typeStr = "EOF_TOKEN"; break;
        }

        // Escape special characters in value
        std::string escapedValue;
        for (char c : value) {
            switch (c) {
            case '\n': escapedValue += "\\n"; break;
            case '\t': escapedValue += "\\t"; break;
            case '\r': escapedValue += "\\r"; break;
            case '\\': escapedValue += "\\\\"; break;
            case '"': escapedValue += "\\\""; break;
            default: escapedValue += c; break;
            }
        }

        ss << "[" << typeStr << " \"" << escapedValue << "\" at " << filename << ":"
            << line << ":" << column << "]";
        return ss.str();
    }

} // namespace lexer