#include "test_framework.h"
#include "../src/lexer/lexer.h"

TEST_SUITE(Lexer)

TEST(Lexer, TokenizeBasic) {
    Lexer lexer("var x = 42;");
    auto tokens = lexer.tokenize();
    
    ASSERT_TRUE(tokens.size() > 0);
    ASSERT_EQ(tokens[0].type, TokenType::KEYWORD);
}

TEST(Lexer, TokenizeString) {
    Lexer lexer("\"hello world\"");
    auto tokens = lexer.tokenize();
    
    ASSERT_TRUE(tokens.size() > 0);
    ASSERT_EQ(tokens[0].type, TokenType::STRING);
}

TEST(Lexer, TokenizeNumber) {
    Lexer lexer("42 3.14");
    auto tokens = lexer.tokenize();
    
    ASSERT_TRUE(tokens.size() >= 2);
    ASSERT_EQ(tokens[0].type, TokenType::NUMBER);
}

TEST(Lexer, TokenizeOperators) {
    Lexer lexer("+ - * / == != < >");
    auto tokens = lexer.tokenize();
    
    ASSERT_TRUE(tokens.size() > 0);
}

TEST(Lexer, TokenizeIdentifiers) {
    Lexer lexer("foo bar_baz camelCase");
    auto tokens = lexer.tokenize();
    
    ASSERT_TRUE(tokens.size() >= 3);
    ASSERT_EQ(tokens[0].type, TokenType::IDENTIFIER);
}
