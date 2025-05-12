void TypeChecker::registerBuiltins()
{
    // Register built-in types
    auto intType = std::make_shared<ast::SimpleType>(
        lexer::Token(lexer::TokenType::IDENTIFIER, "int", "", 0, 0));
    intType->toString() = "int";

    auto floatType = std::make_shared<ast::SimpleType>(
        lexer::Token(lexer::TokenType::IDENTIFIER, "float", "", 0, 0));
    floatType->toString() = "float";

    auto boolType = std::make_shared<ast::SimpleType>(
        lexer::Token(lexer::TokenType::IDENTIFIER, "bool", "", 0, 0));
    boolType->toString() = "bool";

    auto stringType = std::make_shared<ast::SimpleType>(
        lexer::Token(lexer::TokenType::IDENTIFIER, "string", "", 0, 0));
    stringType->toString() = "string";

    auto voidType = std::make_shared<ast::SimpleType>(
        lexer::Token(lexer::TokenType::IDENTIFIER, "void", "", 0, 0));
    voidType->toString() = "void";

    // Register built-in functions
    // Example: print(string) -> void
    std::vector<ast::Parameter> printParams = {
        ast::Parameter("text", stringType)};
    auto printFuncType = std::make_shared<ast::FunctionType>(
        lexer::Token(lexer::TokenType::IDENTIFIER, "print", "", 0, 0),
        std::vector<ast::TypePtr>{stringType}, voidType);
    // Define in the environment
    environment.define("print", printFuncType);
}
} // namespace type_checker
