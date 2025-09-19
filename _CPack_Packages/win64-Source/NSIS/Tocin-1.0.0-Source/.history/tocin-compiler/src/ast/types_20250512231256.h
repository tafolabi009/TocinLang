#pragma once

#include "../lexer/token.h"
#include <memory>
#include <string>

namespace ast
{

    // Forward declarations
    class Type;
    using TypePtr = std::shared_ptr<Type>;

    /**
     * @brief Enumeration of basic type kinds.
     */
    enum class TypeKind
    {
        VOID,
        BOOL,
        INT,
        FLOAT,
        CHAR,
        STRING,
        ARRAY,
        MAP,
        FUNCTION,
        CLASS,
        INTERFACE,
        POINTER,
        REFERENCE,
        UNKNOWN
    };

    /**
     * @brief Base class for all type nodes
     */
    class Type
    {
    public:
        explicit Type(const lexer::Token &token) : token(token) {}
        virtual ~Type() = default;

        virtual std::string toString() const = 0;

        const lexer::Token &getToken() const { return token; }

    protected:
        lexer::Token token;
    };

    /**
     * @brief Represents a basic type like int, bool, string, etc.
     */
    class BasicType : public Type
    {
    public:
        BasicType(TypeKind kind, const lexer::Token &token = lexer::Token())
            : Type(token), kind(kind) {}

        std::string toString() const override
        {
            switch (kind)
            {
            case TypeKind::VOID:
                return "void";
            case TypeKind::BOOL:
                return "bool";
            case TypeKind::INT:
                return "int";
            case TypeKind::FLOAT:
                return "float";
            case TypeKind::CHAR:
                return "char";
            case TypeKind::STRING:
                return "string";
            case TypeKind::ARRAY:
                return "array";
            case TypeKind::MAP:
                return "map";
            case TypeKind::FUNCTION:
                return "function";
            case TypeKind::CLASS:
                return "class";
            case TypeKind::INTERFACE:
                return "interface";
            case TypeKind::POINTER:
                return "pointer";
            case TypeKind::REFERENCE:
                return "reference";
            case TypeKind::UNKNOWN:
                return "unknown";
            default:
                return "unknown";
            }
        }

        TypeKind getKind() const { return kind; }

    private:
        TypeKind kind;
    };

} // namespace ast
