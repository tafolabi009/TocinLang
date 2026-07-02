#include "test_framework.h"
#include "../src/lexer/lexer.h"
#include "../src/parser/parser.h"
#include "../src/type/type_checker.h"
#include "../src/compiler/compilation_context.h"
#include "../src/error/error_handler.h"

// Lex + parse + type-check a source string; report whether any ERROR/FATAL
// diagnostics were produced. This drives the same strict-by-default checking
// the compiler driver gates on (undeclared identifiers, arity mismatches,
// operator/return violations).
static bool checkSource(const std::string &src)
{
    lexer::Lexer lex(src, "test.to");
    auto toks = lex.tokenize();
    parser::Parser parser(toks);
    auto program = parser.parse();
    error::ErrorHandler eh("test.to");
    if (!program)
        return true;
    tocin::compiler::CompilationContext ctx("test.to");
    type_checker::TypeChecker checker(eh, ctx);
    checker.check(program);
    return eh.hasErrors();
}

TEST_SUITE(TypeChecker)

TEST(TypeChecker, ValidProgramHasNoErrors) {
    ASSERT_TRUE(!checkSource(
        "def add(a: int, b: int) -> int { return a + b; }\n"
        "def main() -> int {\n"
        "    let x = 5;\n"
        "    let y = add(x, 2);\n"
        "    print(intToStr(y));\n"
        "    return 0;\n"
        "}\n"));
}

TEST(TypeChecker, UndeclaredIdentifierIsAnError) {
    ASSERT_TRUE(checkSource(
        "def main() -> int {\n"
        "    let count = 5;\n"
        "    return cuont;\n"   // typo must be caught at compile time
        "}\n"));
}

TEST(TypeChecker, AssignmentToUndeclaredIsAnError) {
    ASSERT_TRUE(checkSource(
        "def main() -> int {\n"
        "    total = 5;\n"      // assignment does not auto-declare
        "    return 0;\n"
        "}\n"));
}

TEST(TypeChecker, BuiltinArityMismatchIsAnError) {
    ASSERT_TRUE(checkSource(
        "def main() -> int {\n"
        "    return strLen(\"a\", \"b\");\n"  // strLen takes 1 argument
        "}\n"));
}

TEST(TypeChecker, FunctionArityMismatchIsAnError) {
    ASSERT_TRUE(checkSource(
        "def add(a: int, b: int) -> int { return a + b; }\n"
        "def main() -> int { return add(1); }\n"));
}

TEST(TypeChecker, MethodBodiesResolveSelfAndSiblings) {
    ASSERT_TRUE(!checkSource(
        "class Counter {\n"
        "    let value: int = 0;\n"
        "    def bump(self) -> int { return self.step(); }\n"  // later sibling
        "    def step(self) -> int { return 1; }\n"
        "}\n"
        "def main() -> int { return 0; }\n"));
}

TEST(TypeChecker, EnumVariantsResolveBeforeDeclaration) {
    ASSERT_TRUE(!checkSource(
        "def pick() -> int { let c = Red; return 0; }\n"  // used before decl
        "enum Color { Red, Green, Blue }\n"
        "def main() -> int { return pick(); }\n"));
}

TEST(TypeChecker, CompilerInternalNamesAreAllowed) {
    // "__"-prefixed names are reserved for parser desugaring (__tuple,
    // __tupleGet, __slice, __gen_acc) and must not be flagged.
    ASSERT_TRUE(!checkSource(
        "def main() -> int { let t = __tuple(1, 2); return 0; }\n"));
}

TEST(TypeChecker, ReturnTypeMismatchIsAnError) {
    ASSERT_TRUE(checkSource(
        "def f() -> int { return \"hello\"; }\n"
        "def main() -> int { return f(); }\n"));
}

TEST(TypeChecker, StringTimesIntIsAnError) {
    ASSERT_TRUE(checkSource(
        "def main() -> int {\n"
        "    let s = \"abc\" * 3;\n"
        "    return 0;\n"
        "}\n"));
}
