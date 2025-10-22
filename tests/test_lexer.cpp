#include "test_framework.h"
#include "../src/lexer/lexer.h"

using namespace lexer;

TEST_SUITE(Lexer)

TEST(Lexer, TokenizeBasic) {
    lexer::Lexer lexer("var x = 42;", "test.to");
    auto tokens = lexer.tokenize();
    
    ASSERT_TRUE(tokens.size() > 0);
}

TEST(Lexer, TokenizeString) {
    lexer::Lexer lexer("\"hello world\"", "test.to");
    auto tokens = lexer.tokenize();
    
    ASSERT_TRUE(tokens.size() > 0);
}

TEST(Lexer, TokenizeNumber) {
    lexer::Lexer lexer("42 3.14", "test.to");
    auto tokens = lexer.tokenize();
    
    ASSERT_TRUE(tokens.size() >= 2);
}

TEST(Lexer, TokenizeOperators) {
    lexer::Lexer lexer("+ - * / == != < >", "test.to");
    auto tokens = lexer.tokenize();
    
    ASSERT_TRUE(tokens.size() > 0);
}

TEST(Lexer, TokenizeIdentifiers) {
    lexer::Lexer lexer("foo bar_baz camelCase", "test.to");
    auto tokens = lexer.tokenize();
    
    ASSERT_TRUE(tokens.size() >= 3);
}
