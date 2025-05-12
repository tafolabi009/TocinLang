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
 