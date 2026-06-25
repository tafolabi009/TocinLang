#include "test_framework.h"
#include "../src/lexer/lexer.h"
#include "../src/compiler/macro_system.h"
#include "../src/error/error_handler.h"

using namespace lexer;

// Lex source, run the token-level macro expander, and return the token values
// (excluding EOF) joined by spaces — a compact form to assert against.
static std::string expandToString(const std::string &src, bool &hadError) {
    Lexer lex(src, "test.to");
    auto toks = lex.tokenize();
    error::ErrorHandler eh("test.to");
    auto out = compiler::expandMacroTokens(toks, eh);
    hadError = eh.hasErrors();
    std::string s;
    for (const auto &t : out) {
        if (t.type == TokenType::EOF_TOKEN) continue;
        if (!s.empty()) s += ' ';
        s += t.value;
    }
    return s;
}

TEST_SUITE(Macro)

TEST(Macro, DefinitionIsRemoved) {
    bool err = false;
    auto s = expandToString("macro id(x) { x }\nlet y = 1;", err);
    ASSERT_TRUE(!err);
    // The definition tokens must not survive into the output.
    ASSERT_TRUE(s.find("macro") == std::string::npos);
    ASSERT_TRUE(s.find("let y = 1 ;") != std::string::npos);
}

TEST(Macro, InvocationIsExpandedWithParens) {
    bool err = false;
    auto s = expandToString("macro square(x) { x * x }\nlet y = square!(2 + 3);", err);
    ASSERT_TRUE(!err);
    // Argument is parenthesized and the whole body wrapped: ((2 + 3) * (2 + 3)).
    ASSERT_TRUE(s.find("( ( 2 + 3 ) * ( 2 + 3 ) )") != std::string::npos);
    // No bang/invocation tokens remain.
    ASSERT_TRUE(s.find("!") == std::string::npos);
}

TEST(Macro, NestedMacrosExpandToFixedPoint) {
    bool err = false;
    auto s = expandToString(
        "macro square(x) { x * x }\n"
        "macro sumsq(a, b) { square!(a) + square!(b) }\n"
        "let z = sumsq!(3, 4);",
        err);
    ASSERT_TRUE(!err);
    ASSERT_TRUE(s.find("square") == std::string::npos); // fully expanded
    ASSERT_TRUE(s.find("!") == std::string::npos);
}

TEST(Macro, ArgumentCountMismatchIsAnError) {
    bool err = false;
    expandToString("macro add(a, b) { a + b }\nlet q = add!(1);", err);
    ASSERT_TRUE(err);
}

TEST(Macro, SourceWithoutMacrosIsUnchanged) {
    bool err = false;
    auto s = expandToString("def main() { return 1 + 2; }", err);
    ASSERT_TRUE(!err);
    ASSERT_TRUE(s.find("def main ( ) { return 1 + 2 ; }") != std::string::npos);
}
