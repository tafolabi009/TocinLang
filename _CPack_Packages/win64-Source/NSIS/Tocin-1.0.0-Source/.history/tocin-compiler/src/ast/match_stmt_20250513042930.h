#pragma once

#include "ast.h"

// This file defines pattern matching related classes needed by IR generator
// While it includes ast.h for the base Statement/Expression classes,
// it adds specific pattern matching functionality

namespace ast
{
    // Forward declarations
    class Pattern;
    class MatchCase;
    
    using PatternPtr = std::shared_ptr<Pattern>;
    using MatchCasePtr = std::shared_ptr<MatchCase>;

    /**
     * Represents a pattern in a match expression
     */
    class Pattern
    {
    public:
        enum class Kind
        {
            WILDCARD,    // _ (matches anything)
            LITERAL,     // 42, "hello", true, etc.
            VARIABLE,    // x (binds the matched value to x)
            CONSTRUCTOR, // Some(x), Ok(value), etc.
            TUPLE,       // (a, b, c)
            STRUCT,      // Person { name, age }
            OR           // pattern1 | pattern2
        };

        explicit Pattern(Kind kind, const lexer::Token &token)
            : kind(kind), token(token) {}

        virtual ~Pattern() = default;

        Kind getKind() const { return kind; }
        const lexer::Token &getToken() const { return token; }

        // Whether this pattern binds any variables
        virtual bool bindsVariables() const = 0;

        // Names of variables bound by this pattern
        virtual std::vector<std::string> boundVariables() const = 0;

    private:
        Kind kind;
        lexer::Token token;
    };

    /**
     * Wildcard pattern (_) that matches anything but binds nothing
     */
    class WildcardPattern : public Pattern
    {
    public:
        explicit WildcardPattern(const lexer::Token &token)
            : Pattern(Kind::WILDCARD, token) {}

        bool bindsVariables() const override { return false; }
        std::vector<std::string> boundVariables() const override { return {}; }
    };

    /**
     * Literal pattern that matches a specific value
     */
    class LiteralPattern : public Pattern
    {
    public:
        LiteralPattern(const lexer::Token &token, ExprPtr literal)
            : Pattern(Kind::LITERAL, token), literal(std::move(literal)) {}

        const ExprPtr &getLiteral() const { return literal; }

        bool bindsVariables() const override { return false; }
        std::vector<std::string> boundVariables() const override { return {}; }

    private:
        ExprPtr literal;
    };

    /**
     * Variable pattern that binds the matched value to a variable
     */
    class VariablePattern : public Pattern
    {
    public:
        VariablePattern(const lexer::Token &token, std::string name)
            : Pattern(Kind::VARIABLE, token), name(std::move(name)) {}

        const std::string &getName() const { return name; }

        bool bindsVariables() const override { return true; }
        std::vector<std::string> boundVariables() const override { return {name}; }

    private:
        std::string name;
    };

    /**
     * Constructor pattern that matches a specific variant and extracts its contents
     * Examples: Some(x), None, Ok(value), Err(e)
     */
    class ConstructorPattern : public Pattern
    {
    public:
        ConstructorPattern(const lexer::Token &token, std::string name,
                           std::vector<PatternPtr> arguments)
            : Pattern(Kind::CONSTRUCTOR, token), name(std::move(name)),
              arguments(std::move(arguments)) {}

        const std::string &getName() const { return name; }
        const std::vector<PatternPtr> &getArguments() const { return arguments; }

        bool bindsVariables() const override;
        std::vector<std::string> boundVariables() const override;

    private:
        std::string name;
        std::vector<PatternPtr> arguments;
    };

    /**
     * Tuple pattern that matches a tuple and extracts its elements
     * Example: (x, y, z)
     */
    class TuplePattern : public Pattern
    {
    public:
        TuplePattern(const lexer::Token &token, std::vector<PatternPtr> elements)
            : Pattern(Kind::TUPLE, token), elements(std::move(elements)) {}

        const std::vector<PatternPtr> &getElements() const { return elements; }

        bool bindsVariables() const override;
        std::vector<std::string> boundVariables() const override;

    private:
        std::vector<PatternPtr> elements;
    };

    /**
     * Struct pattern that matches a struct and extracts its fields
     * Example: Person { name, age: 30 }
     */
    class StructPattern : public Pattern
    {
    public:
        struct Field
        {
            std::string name;
            PatternPtr pattern;

            Field(std::string name, PatternPtr pattern)
                : name(std::move(name)), pattern(std::move(pattern)) {}
        };

        StructPattern(const lexer::Token &token, std::string typeName,
                      std::vector<Field> fields)
            : Pattern(Kind::STRUCT, token),
              typeName(std::move(typeName)),
              fields(std::move(fields)) {}

        const std::string &getTypeName() const { return typeName; }
        const std::vector<Field> &getFields() const { return fields; }

        bool bindsVariables() const override;
        std::vector<std::string> boundVariables() const override;

    private:
        std::string typeName;
        std::vector<Field> fields;
    };

    /**
     * OR pattern that matches if either of the patterns match
     * Example: Some(x) | None
     */
    class OrPattern : public Pattern
    {
    public:
        OrPattern(const lexer::Token &token, PatternPtr left, PatternPtr right)
            : Pattern(Kind::OR, token), left(std::move(left)), right(std::move(right)) {}

        const PatternPtr &getLeft() const { return left; }
        const PatternPtr &getRight() const { return right; }

        bool bindsVariables() const override;
        std::vector<std::string> boundVariables() const override;

    private:
        PatternPtr left;
        PatternPtr right;
    };

    /**
     * Represents a case in a match expression
     */
    class MatchCase
    {
    public:
        MatchCase(PatternPtr pattern, StmtPtr body)
            : pattern(std::move(pattern)), body(std::move(body)) {}

        const PatternPtr &getPattern() const { return pattern; }
        const StmtPtr &getBody() const { return body; }

    private:
        PatternPtr pattern;
        StmtPtr body;
    };

    // Use the MatchStmt class defined in ast.h
    // No redefinition needed
}
