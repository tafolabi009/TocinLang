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

        bool equals(const TypePtr &other) const override
        {
            if (auto otherNullable = std::dynamic_pointer_cast<NullableType>(other))
            {
                return baseType->equals(otherNullable->baseType);
            }
            return false;
        }

        TypePtr clone() const override
        {
            return std::make_shared<NullableType>(token, baseType->clone());
        }
    };

    /**
     * @brief Represents a generic type with type parameters
     */
    class GenericType : public Type
    {
    public:
        std::string name;
        std::vector<TypePtr> typeArguments;

        GenericType(const lexer::Token &token, const std::string &name, std::vector<TypePtr> typeArgs)
            : Type(token), name(name), typeArguments(std::move(typeArgs)) {}

        std::string toString() const override
        {
            std::string result = name + "<";
            for (size_t i = 0; i < typeArguments.size(); ++i)
            {
                if (i > 0) result += ", ";
                result += typeArguments[i]->toString();
            }
            result += ">";
            return result;
        }

        bool equals(const TypePtr &other) const override
        {
            if (auto otherGeneric = std::dynamic_pointer_cast<GenericType>(other))
            {
                if (name != otherGeneric->name || typeArguments.size() != otherGeneric->typeArguments.size())
                    return false;
                for (size_t i = 0; i < typeArguments.size(); ++i)
                {
                    if (!typeArguments[i]->equals(otherGeneric->typeArguments[i]))
                        return false;
                }
                return true;
            }
            return false;
        }

        TypePtr clone() const override
        {
            std::vector<TypePtr> clonedArgs;
            for (const auto &arg : typeArguments)
            {
                clonedArgs.push_back(arg->clone());
            }
            return std::make_shared<GenericType>(token, name, std::move(clonedArgs));
        }
    };

    /**
     * @brief Represents a function type
     */
    class FunctionType : public Type
    {
    public:
        std::vector<TypePtr> parameterTypes;
        TypePtr returnType;
        bool isAsync;

        FunctionType(const lexer::Token &token, std::vector<TypePtr> paramTypes, TypePtr retType, bool async = false)
            : Type(token), parameterTypes(std::move(paramTypes)), returnType(std::move(retType)), isAsync(async) {}

        std::string toString() const override
        {
            std::string result = "(";
            for (size_t i = 0; i < parameterTypes.size(); ++i)
            {
                if (i > 0) result += ", ";
                result += parameterTypes[i]->toString();
            }
            result += ") -> ";
            if (isAsync) result += "async ";
            result += returnType->toString();
            return result;
        }

        bool equals(const TypePtr &other) const override
        {
            if (auto otherFunc = std::dynamic_pointer_cast<FunctionType>(other))
            {
                if (parameterTypes.size() != otherFunc->parameterTypes.size() || isAsync != otherFunc->isAsync)
                    return false;
                if (!returnType->equals(otherFunc->returnType))
                    return false;
                for (size_t i = 0; i < parameterTypes.size(); ++i)
                {
                    if (!parameterTypes[i]->equals(otherFunc->parameterTypes[i]))
                        return false;
                }
                return true;
            }
            return false;
        }

        TypePtr clone() const override
        {
            std::vector<TypePtr> clonedParams;
            for (const auto &param : parameterTypes)
            {
                clonedParams.push_back(param->clone());
            }
            return std::make_shared<FunctionType>(token, std::move(clonedParams), returnType->clone(), isAsync);
        }
    };

    /**
     * @brief Represents a tuple type
     */
    class TupleType : public Type
    {
    public:
        std::vector<TypePtr> elementTypes;

        TupleType(const lexer::Token &token, std::vector<TypePtr> elements)
            : Type(token), elementTypes(std::move(elements)) {}

        std::string toString() const override
        {
            std::string result = "(";
            for (size_t i = 0; i < elementTypes.size(); ++i)
            {
                if (i > 0) result += ", ";
                result += elementTypes[i]->toString();
            }
            result += ")";
            return result;
        }

        bool equals(const TypePtr &other) const override
        {
            if (auto otherTuple = std::dynamic_pointer_cast<TupleType>(other))
            {
                if (elementTypes.size() != otherTuple->elementTypes.size())
                    return false;
                for (size_t i = 0; i < elementTypes.size(); ++i)
                {
                    if (!elementTypes[i]->equals(otherTuple->elementTypes[i]))
                        return false;
                }
                return true;
            }
            return false;
        }

        TypePtr clone() const override
        {
            std::vector<TypePtr> clonedElements;
            for (const auto &element : elementTypes)
            {
                clonedElements.push_back(element->clone());
            }
            return std::make_shared<TupleType>(token, std::move(clonedElements));
        }
    };

    /**
     * @brief Represents an array type
     */
    class ArrayType : public Type
    {
    public:
        TypePtr elementType;
        int size; // -1 for dynamic arrays

        ArrayType(const lexer::Token &token, TypePtr elemType, int arraySize = -1)
            : Type(token), elementType(std::move(elemType)), size(arraySize) {}

        std::string toString() const override
        {
            if (size >= 0)
            {
                return elementType->toString() + "[" + std::to_string(size) + "]";
            }
            return elementType->toString() + "[]";
        }

        bool equals(const TypePtr &other) const override
        {
            if (auto otherArray = std::dynamic_pointer_cast<ArrayType>(other))
            {
                return size == otherArray->size && elementType->equals(otherArray->elementType);
            }
            return false;
        }

        TypePtr clone() const override
        {
            return std::make_shared<ArrayType>(token, elementType->clone(), size);
        }
    };

    /**
     * @brief Represents a pointer type
     */
    class PointerType : public Type
    {
    public:
        TypePtr pointeeType;

        PointerType(const lexer::Token &token, TypePtr pointee)
            : Type(token), pointeeType(std::move(pointee)) {}

        std::string toString() const override
        {
            return "*" + pointeeType->toString();
        }

        bool equals(const TypePtr &other) const override
        {
            if (auto otherPtr = std::dynamic_pointer_cast<PointerType>(other))
            {
                return pointeeType->equals(otherPtr->pointeeType);
            }
            return false;
        }

        TypePtr clone() const override
        {
            return std::make_shared<PointerType>(token, pointeeType->clone());
        }
    };

    /**
     * @brief Represents a reference type
     */
    class ReferenceType : public Type
    {
    public:
        TypePtr referencedType;
        bool isMutable;

        ReferenceType(const lexer::Token &token, TypePtr referenced, bool mutable_ = true)
            : Type(token), referencedType(std::move(referenced)), isMutable(mutable_) {}

        std::string toString() const override
        {
            return "&" + (isMutable ? "mut " : "") + referencedType->toString();
        }

        bool equals(const TypePtr &other) const override
        {
            if (auto otherRef = std::dynamic_pointer_cast<ReferenceType>(other))
            {
                return isMutable == otherRef->isMutable && referencedType->equals(otherRef->referencedType);
            }
            return false;
        }

        TypePtr clone() const override
        {
            return std::make_shared<ReferenceType>(token, referencedType->clone(), isMutable);
        }
    };

    /**
     * @brief Represents an Option type (Option<T>)
     */
    class OptionType : public Type
    {
    public:
        TypePtr innerType;

        OptionType(const lexer::Token &token, TypePtr inner)
            : Type(token), innerType(std::move(inner)) {}

        std::string toString() const override
        {
            return "Option<" + innerType->toString() + ">";
        }

        bool equals(const TypePtr &other) const override
        {
            if (auto otherOption = std::dynamic_pointer_cast<OptionType>(other))
            {
                return innerType->equals(otherOption->innerType);
            }
            return false;
        }

        TypePtr clone() const override
        {
            return std::make_shared<OptionType>(token, innerType->clone());
        }
    };

    /**
     * @brief Represents a Result type (Result<T, E>)
     */
    class ResultType : public Type
    {
    public:
        TypePtr okType;
        TypePtr errorType;

        ResultType(const lexer::Token &token, TypePtr ok, TypePtr error)
            : Type(token), okType(std::move(ok)), errorType(std::move(error)) {}

        std::string toString() const override
        {
            return "Result<" + okType->toString() + ", " + errorType->toString() + ">";
        }

        bool equals(const TypePtr &other) const override
        {
            if (auto otherResult = std::dynamic_pointer_cast<ResultType>(other))
            {
                return okType->equals(otherResult->okType) && errorType->equals(otherResult->errorType);
            }
            return false;
        }

        TypePtr clone() const override
        {
            return std::make_shared<ResultType>(token, okType->clone(), errorType->clone());
        }
    };

    /**
     * @brief Represents a trait type
     */
    class TraitType : public Type
    {
    public:
        std::string name;
        std::vector<TypePtr> typeArguments;

        TraitType(const lexer::Token &token, const std::string &traitName, std::vector<TypePtr> typeArgs = {})
            : Type(token), name(traitName), typeArguments(std::move(typeArgs)) {}

        std::string toString() const override
        {
            if (typeArguments.empty())
            {
                return "trait " + name;
            }
            std::string result = "trait " + name + "<";
            for (size_t i = 0; i < typeArguments.size(); ++i)
            {
                if (i > 0) result += ", ";
                result += typeArguments[i]->toString();
            }
            result += ">";
            return result;
        }

        bool equals(const TypePtr &other) const override
        {
            if (auto otherTrait = std::dynamic_pointer_cast<TraitType>(other))
            {
                if (name != otherTrait->name || typeArguments.size() != otherTrait->typeArguments.size())
                    return false;
                for (size_t i = 0; i < typeArguments.size(); ++i)
                {
                    if (!typeArguments[i]->equals(otherTrait->typeArguments[i]))
                        return false;
                }
                return true;
            }
            return false;
        }

        TypePtr clone() const override
        {
            std::vector<TypePtr> clonedArgs;
            for (const auto &arg : typeArguments)
            {
                clonedArgs.push_back(arg->clone());
            }
            return std::make_shared<TraitType>(token, name, std::move(clonedArgs));
        }
    };

    /**
     * @brief Represents a channel type for concurrency
     */
    class ChannelType : public Type
    {
    public:
        TypePtr elementType;
        bool isSend;
        bool isReceive;

        ChannelType(const lexer::Token &token, TypePtr elemType, bool send = true, bool receive = true)
            : Type(token), elementType(std::move(elemType)), isSend(send), isReceive(receive) {}

        std::string toString() const override
        {
            std::string prefix = "chan";
            if (isSend && !isReceive) prefix = "chan<-";
            else if (!isSend && isReceive) prefix = "<-chan";
            return prefix + " " + elementType->toString();
        }

        bool equals(const TypePtr &other) const override
        {
            if (auto otherChan = std::dynamic_pointer_cast<ChannelType>(other))
            {
                return isSend == otherChan->isSend &&
                       isReceive == otherChan->isReceive &&
                       elementType->equals(otherChan->elementType);
            }
            return false;
        }

        TypePtr clone() const override
        {
            return std::make_shared<ChannelType>(token, elementType->clone(), isSend, isReceive);
        }
    };

    // Forward declarations for AST nodes
    class Value;
    class Expression;
    class Statement;
    class FunctionDecl;
    class ClassDecl;
    class TraitDecl;
    
    using ValuePtr = std::shared_ptr<Value>;
    using ExprPtr = std::shared_ptr<Expression>;
    using StmtPtr = std::shared_ptr<Statement>;
    using FunctionDeclPtr = std::shared_ptr<FunctionDecl>;
    using ClassDeclPtr = std::shared_ptr<ClassDecl>;
    using TraitDeclPtr = std::shared_ptr<TraitDecl>;

} // namespace ast
