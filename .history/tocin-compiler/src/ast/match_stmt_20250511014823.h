#pragma once

#include "stmt.h"
#include "expr.h"
#include "../lexer/token.h"
#include <memory>
#include <vector>
#include <string>

namespace ast
{

    // Forward declarations
    class Visitor;

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

    using PatternPtr = std::shared_ptr<Pattern>;

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

    using MatchCasePtr = std::shared_ptr<MatchCase>;

    /**
     * Represents a match statement (similar to switch but with pattern matching)
     * match expression {
     *   pattern1 => statement1,
     *   pattern2 => statement2,
     *   _ => default_statement
     * }
     */
    class MatchStmt : public Stmt
    {
    public:
        MatchStmt(const lexer::Token &token, ExprPtr expression,
                  std::vector<MatchCasePtr> cases)
            : Stmt(token), expression(std::move(expression)), cases(std::move(cases)) {}

        void accept(Visitor &visitor) override;

        const ExprPtr &getExpression() const { return expression; }
        const std::vector<MatchCasePtr> &getCases() const { return cases; }

    private:
        ExprPtr expression;              // The expression being matched
        std::vector<MatchCasePtr> cases; // The match cases
    };

} // namespace ast
