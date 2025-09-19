void IRGenerator::visitLiteralExpr(ast::LiteralExpr *expr)
{
    if (expr->value.getType() == lexer::TokenType::INT_LITERAL)
    {
        // Convert string to int64
        int64_t value = std::stoll(expr->value.lexeme);
        lastValue = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), value);
    }
    else if (expr->value.getType() == lexer::TokenType::FLOAT_LITERAL)
    {
        // Convert string to double
        double value = std::stod(expr->value.lexeme);
        lastValue = llvm::ConstantFP::get(llvm::Type::getDoubleTy(context), value);
    }
    else if (expr->value.getType() == lexer::TokenType::STRING_LITERAL)
 