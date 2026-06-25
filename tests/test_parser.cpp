#include "test_framework.h"
#include "../src/parser/parser.h"
#include "../src/ast/ast.h"

using namespace lexer;
using namespace parser;

// Parse source into an AST root.
static ast::StmtPtr parseProgram(const std::string &src) {
    Lexer lex(src, "test.to");
    auto tokens = lex.tokenize();
    Parser parser(tokens);
    return parser.parse();
}

// Collect the top-level statements regardless of how the root is wrapped.
static std::vector<ast::StmtPtr> topLevel(const ast::StmtPtr &root) {
    if (auto blk = std::dynamic_pointer_cast<ast::BlockStmt>(root))
        return blk->statements;
    if (auto mod = std::dynamic_pointer_cast<ast::ModuleStmt>(root))
        return mod->body;
    if (root) return {root};
    return {};
}

TEST_SUITE(Parser)

TEST(Parser, ParsesFunctionDeclaration) {
    auto root = parseProgram("def add(a: int, b: int) -> int { return a + b; }");
    ASSERT_TRUE(root != nullptr);
    auto stmts = topLevel(root);
    ASSERT_TRUE(!stmts.empty());
    auto fn = std::dynamic_pointer_cast<ast::FunctionStmt>(stmts[0]);
    ASSERT_TRUE(fn != nullptr);
    ASSERT_EQ(std::string("add"), fn->name);
    ASSERT_EQ(size_t(2), fn->parameters.size());
}

TEST(Parser, InfersNullReturnTypeWhenUnannotated) {
    auto root = parseProgram("def f() { return 1; }");
    auto fn = std::dynamic_pointer_cast<ast::FunctionStmt>(topLevel(root)[0]);
    ASSERT_TRUE(fn != nullptr);
    ASSERT_TRUE(fn->returnType == nullptr); // left null for inference
}

TEST(Parser, ParsesVariableDeclaration) {
    auto root = parseProgram("def m() { let x: int = 42; }");
    auto fn = std::dynamic_pointer_cast<ast::FunctionStmt>(topLevel(root)[0]);
    ASSERT_TRUE(fn != nullptr);
    auto body = std::dynamic_pointer_cast<ast::BlockStmt>(fn->body);
    ASSERT_TRUE(body != nullptr);
    auto var = std::dynamic_pointer_cast<ast::VariableStmt>(body->statements[0]);
    ASSERT_TRUE(var != nullptr);
    ASSERT_EQ(std::string("x"), var->name);
}

TEST(Parser, ParsesIfElifElse) {
    auto root = parseProgram(
        "def m() { if 1 { return 1; } elif 2 { return 2; } else { return 3; } }");
    auto fn = std::dynamic_pointer_cast<ast::FunctionStmt>(topLevel(root)[0]);
    auto body = std::dynamic_pointer_cast<ast::BlockStmt>(fn->body);
    auto iff = std::dynamic_pointer_cast<ast::IfStmt>(body->statements[0]);
    ASSERT_TRUE(iff != nullptr);
    ASSERT_EQ(size_t(1), iff->elifBranches.size());
    ASSERT_TRUE(iff->elseBranch != nullptr);
}

TEST(Parser, ParsesWhileLoop) {
    auto root = parseProgram("def m() { while 1 { return 0; } }");
    auto fn = std::dynamic_pointer_cast<ast::FunctionStmt>(topLevel(root)[0]);
    auto body = std::dynamic_pointer_cast<ast::BlockStmt>(fn->body);
    ASSERT_TRUE(std::dynamic_pointer_cast<ast::WhileStmt>(body->statements[0]) != nullptr);
}

TEST(Parser, ParsesForRange) {
    auto root = parseProgram("def m() { for i in 0..10 { return i; } }");
    auto fn = std::dynamic_pointer_cast<ast::FunctionStmt>(topLevel(root)[0]);
    auto body = std::dynamic_pointer_cast<ast::BlockStmt>(fn->body);
    auto loop = std::dynamic_pointer_cast<ast::ForStmt>(body->statements[0]);
    ASSERT_TRUE(loop != nullptr);
    ASSERT_EQ(std::string("i"), loop->variable);
}

TEST(Parser, ParsesClassWithFieldAndMethod) {
    auto root = parseProgram(
        "class Point { x: int; y: int; def sum(self) -> int { return self.x + self.y; } }");
    auto cls = std::dynamic_pointer_cast<ast::ClassStmt>(topLevel(root)[0]);
    ASSERT_TRUE(cls != nullptr);
    ASSERT_EQ(std::string("Point"), cls->name);
    ASSERT_EQ(size_t(2), cls->fields.size());
    ASSERT_EQ(size_t(1), cls->methods.size());
}

TEST(Parser, ParsesMatchStatement) {
    auto root = parseProgram(
        "def m() { match 1 { case 1: { return 1; } default: { return 0; } } }");
    auto fn = std::dynamic_pointer_cast<ast::FunctionStmt>(topLevel(root)[0]);
    auto body = std::dynamic_pointer_cast<ast::BlockStmt>(fn->body);
    auto m = std::dynamic_pointer_cast<ast::MatchStmt>(body->statements[0]);
    ASSERT_TRUE(m != nullptr);
    ASSERT_EQ(size_t(1), m->cases.size());
    ASSERT_TRUE(m->defaultCase != nullptr);
}

TEST(Parser, ParsesReturnWithBinaryExpr) {
    auto root = parseProgram("def m() { return 2 + 3 * 4; }");
    auto fn = std::dynamic_pointer_cast<ast::FunctionStmt>(topLevel(root)[0]);
    auto body = std::dynamic_pointer_cast<ast::BlockStmt>(fn->body);
    auto ret = std::dynamic_pointer_cast<ast::ReturnStmt>(body->statements[0]);
    ASSERT_TRUE(ret != nullptr);
    ASSERT_TRUE(std::dynamic_pointer_cast<ast::BinaryExpr>(ret->value) != nullptr);
}

TEST(Parser, ParsesMultipleTopLevelDeclarations) {
    auto root = parseProgram("def a() { return 1; } def b() { return 2; }");
    auto stmts = topLevel(root);
    ASSERT_TRUE(stmts.size() >= 2);
}

TEST(Parser, ParsesTryCatchFinally) {
    auto root = parseProgram(
        "def m() { try { throw 1; } catch (e) { return e; } finally { return 0; } }");
    auto fn = std::dynamic_pointer_cast<ast::FunctionStmt>(topLevel(root)[0]);
    auto body = std::dynamic_pointer_cast<ast::BlockStmt>(fn->body);
    auto tryStmt = std::dynamic_pointer_cast<ast::TryStmt>(body->statements[0]);
    ASSERT_TRUE(tryStmt != nullptr);
    ASSERT_TRUE(tryStmt->tryBlock != nullptr);
    ASSERT_EQ(std::string("e"), tryStmt->catchVar);
    ASSERT_TRUE(tryStmt->catchBlock != nullptr);
    ASSERT_TRUE(tryStmt->finallyBlock != nullptr);
}

TEST(Parser, ParsesThrowStatement) {
    auto root = parseProgram("def m() { throw 42; }");
    auto fn = std::dynamic_pointer_cast<ast::FunctionStmt>(topLevel(root)[0]);
    auto body = std::dynamic_pointer_cast<ast::BlockStmt>(fn->body);
    auto thr = std::dynamic_pointer_cast<ast::ThrowStmt>(body->statements[0]);
    ASSERT_TRUE(thr != nullptr);
    ASSERT_TRUE(thr->value != nullptr);
}
