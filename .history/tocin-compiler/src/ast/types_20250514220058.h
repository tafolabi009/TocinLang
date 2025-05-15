#pragma once

#include "../lexer/token.h"
#include <memory>
#include <string>
#include <vector>

namespace ast
{

    // Forward declarations
    class Type;
    class BasicType;
    class GenericType;
    class FunctionType;
    class TupleType;
    class ArrayType;
    class PointerType;
    class ReferenceType;
    class NullableType;
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

        // Add virtual methods for type comparison and cloning
        virtual bool equals(const TypePtr &other) const { return this == other.get(); }
        virtual TypePtr clone() const = 0;

        // Make token public so it can be accessed by type_checker
        lexer::Token token;
    };

    /**
     * @brief Represents a basic type like int, bool, string, etc.
     */
    class BasicType : public Type
    {
    public:
        // Use default-constructed token directly instead of as a default parameter
        BasicType(TypeKind kind)
            : Type(lexer::Token()), kind(kind) {}

        BasicType(TypeKind kind, const lexer::Token &token)
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

        TypePtr clone() const override
        {
            return std::make_shared<BasicType>(kind, token);
        }

    private:
        TypeKind kind;
    };

    /**
     * Represents a nullable type (Type?)
     */
    class NullableType : public Type
    {
    public:
        TypePtr baseType; // The type that can be null

        NullableType(const lexer::Token &token, TypePtr baseType)
            : Type(token), baseType(std::move(baseType)) {}

        std::string toString() const override
        {
            return baseType->toString() + "?";
        }
    };

} // namespace ast
