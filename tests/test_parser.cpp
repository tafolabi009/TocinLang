#include "test_framework.h"
#include "../src/parser/parser.h"

using namespace lexer;
using namespace parser;

TEST_SUITE(Parser)

TEST(Parser, ParseVariableDeclaration) {
    lexer::Lexer lexer("var x: int = 42;", "test.to");
    auto tokens = lexer.tokenize();
    parser::Parser parser(tokens);
    
    auto ast = parser.parse();
    ASSERT_TRUE(ast != nullptr);
}

TEST(Parser, ParseFunctionDeclaration) {
    lexer::Lexer lexer("fn add(a: int, b: int): int { return a + b; }", "test.to");
    auto tokens = lexer.tokenize();
    parser::Parser parser(tokens);
    
    auto ast = parser.parse();
    ASSERT_TRUE(ast != nullptr);
}

TEST(Parser, ParseIfStatement) {
    lexer::Lexer lexer("if (x > 0) { print(x); }", "test.to");
    auto tokens = lexer.tokenize();
    parser::Parser parser(tokens);
    
    auto ast = parser.parse();
    ASSERT_TRUE(ast != nullptr);
}

TEST(Parser, ParseWhileLoop) {
    lexer::Lexer lexer("while (i < 10) { i = i + 1; }", "test.to");
    auto tokens = lexer.tokenize();
    parser::Parser parser(tokens);
    
    auto ast = parser.parse();
    ASSERT_TRUE(ast != nullptr);
}

TEST(Parser, ParseBinaryExpression) {
    lexer::Lexer lexer("x + y * z", "test.to");
    auto tokens = lexer.tokenize();
    parser::Parser parser(tokens);
    
    auto ast = parser.parse();
    ASSERT_TRUE(ast != nullptr);
}
