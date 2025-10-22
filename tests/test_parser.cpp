#include "test_framework.h"
#include "../src/parser/parser.h"

TEST_SUITE(Parser)

TEST(Parser, ParseVariableDeclaration) {
    Lexer lexer("var x: int = 42;");
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    
    auto ast = parser.parse();
    ASSERT_TRUE(ast != nullptr);
}

TEST(Parser, ParseFunctionDeclaration) {
    Lexer lexer("fn add(a: int, b: int): int { return a + b; }");
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    
    auto ast = parser.parse();
    ASSERT_TRUE(ast != nullptr);
}

TEST(Parser, ParseIfStatement) {
    Lexer lexer("if (x > 0) { print(x); }");
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    
    auto ast = parser.parse();
    ASSERT_TRUE(ast != nullptr);
}

TEST(Parser, ParseWhileLoop) {
    Lexer lexer("while (i < 10) { i = i + 1; }");
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    
    auto ast = parser.parse();
    ASSERT_TRUE(ast != nullptr);
}

TEST(Parser, ParseBinaryExpression) {
    Lexer lexer("x + y * z");
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    
    auto ast = parser.parse();
    ASSERT_TRUE(ast != nullptr);
}
