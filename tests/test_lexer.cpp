#include "test_framework.h"
#include "../src/lexer/lexer.h"

using namespace lexer;

// Count how many tokens of a given type appear in the stream.
static int countType(const std::vector<Token> &toks, TokenType t) {
    int n = 0;
    for (const auto &tok : toks)
        if (tok.type == t) ++n;
    return n;
}

// Find the first token of a given type, or return nullptr.
static const Token *firstOf(const std::vector<Token> &toks, TokenType t) {
    for (const auto &tok : toks)
        if (tok.type == t) return &tok;
    return nullptr;
}

TEST_SUITE(Lexer)

TEST(Lexer, EmitsEofToken) {
    Lexer lex("", "test.to");
    auto toks = lex.tokenize();
    ASSERT_TRUE(!toks.empty());
    ASSERT_TRUE(toks.back().type == TokenType::EOF_TOKEN);
}

TEST(Lexer, KeywordsAreClassified) {
    Lexer lex("def let if return", "test.to");
    auto toks = lex.tokenize();
    ASSERT_EQ(1, countType(toks, TokenType::DEF));
    ASSERT_EQ(1, countType(toks, TokenType::LET));
    ASSERT_EQ(1, countType(toks, TokenType::IF));
    ASSERT_EQ(1, countType(toks, TokenType::RETURN));
}

TEST(Lexer, IdentifiersAreNotKeywords) {
    // 'print' and 'self' must lex as identifiers (regression for the keyword fix).
    Lexer lex("print self foo bar_baz", "test.to");
    auto toks = lex.tokenize();
    ASSERT_TRUE(countType(toks, TokenType::IDENTIFIER) >= 4);
}

TEST(Lexer, IntegerLiteral) {
    Lexer lex("42", "test.to");
    auto toks = lex.tokenize();
    const Token *t = firstOf(toks, TokenType::INT);
    ASSERT_TRUE(t != nullptr);
    ASSERT_EQ(std::string("42"), t->value);
}

TEST(Lexer, FloatLiteral) {
    Lexer lex("3.14", "test.to");
    auto toks = lex.tokenize();
    ASSERT_TRUE(firstOf(toks, TokenType::FLOAT64) != nullptr ||
                firstOf(toks, TokenType::FLOAT32) != nullptr);
}

TEST(Lexer, StringLiteral) {
    Lexer lex("\"hello\"", "test.to");
    auto toks = lex.tokenize();
    ASSERT_TRUE(firstOf(toks, TokenType::STRING) != nullptr);
}

TEST(Lexer, Operators) {
    Lexer lex("+ - * / %", "test.to");
    auto toks = lex.tokenize();
    ASSERT_EQ(1, countType(toks, TokenType::PLUS));
}

TEST(Lexer, Punctuation) {
    Lexer lex("foo()\n", "test.to");
    auto toks = lex.tokenize();
    ASSERT_EQ(1, countType(toks, TokenType::LEFT_PAREN));
    ASSERT_EQ(1, countType(toks, TokenType::RIGHT_PAREN));
}

TEST(Lexer, LastTokenPreservedWithoutTrailingNewline) {
    // Regression: a source with no trailing newline must keep its final token.
    Lexer lex("foo()", "test.to");
    auto toks = lex.tokenize();
    ASSERT_EQ(1, countType(toks, TokenType::RIGHT_PAREN));
}

TEST(Lexer, FunctionSignatureTokenShape) {
    Lexer lex("def add(a: int, b: int) -> int {", "test.to");
    auto toks = lex.tokenize();
    ASSERT_EQ(1, countType(toks, TokenType::DEF));
    ASSERT_EQ(1, countType(toks, TokenType::LEFT_PAREN));
    ASSERT_TRUE(countType(toks, TokenType::IDENTIFIER) >= 3); // add, a, b (and types)
}

TEST(Lexer, TracksLineNumbers) {
    Lexer lex("a\nb\nc", "test.to");
    auto toks = lex.tokenize();
    const Token *c = nullptr;
    for (const auto &tok : toks)
        if (tok.type == TokenType::IDENTIFIER && tok.value == "c") c = &tok;
    ASSERT_TRUE(c != nullptr);
    ASSERT_EQ(3, c->line);
}
