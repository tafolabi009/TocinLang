#pragma once

#include <string>
#include <vector>
#include <memory>

namespace tocin {
namespace ast {

class ASTNode {
public:
    ASTNode() = default;
    virtual ~ASTNode() = default;

    virtual std::string toString() const = 0;
};

class Expression : public ASTNode {
public:
    Expression() = default;
    virtual ~Expression() = default;
};

class Statement : public ASTNode {
public:
    Statement() = default;
    virtual ~Statement() = default;
};

class Program : public ASTNode {
public:
    Program() = default;
    ~Program() override = default;

    std::string toString() const override {
        return "Program";
    }

    std::vector<std::unique_ptr<Statement>> statements;
};

} // namespace ast
} // namespace tocin 