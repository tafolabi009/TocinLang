// tocin-compiler/src/parser/parser.cpp
#include "parser.h"
#include <iostream>
#include <algorithm>

Parser::Parser(const std::vector<Token> &tokens) : tokens(tokens), current(0) {}

// ---------------------------------------------------------------------------
// Public Interface
// ---------------------------------------------------------------------------

std::shared_ptr<ast::Statement> Parser::parse()
{
    try
    {
        return declaration();
    }
    catch (const ParseError &error)
    {
        std::cerr << error.what() << std::endl;
        synchronize();
        return nullptr;
    }
}

// ---------------------------------------------------------------------------
// Helper Methods: Token Handling and Error Reporting
// ---------------------------------------------------------------------------

bool Parser::check(TokenType type) const
{
    return !isAtEnd() && peek().type == type;
}

Token Parser::peek() const
{
    return tokens[current];
}

Token Parser::previous() const
{
    return tokens[current - 1];
}

Token Parser::advance()
{
    if (!isAtEnd())
        current++;
    return previous();
}

bool Parser::isAtEnd() const
{
    return peek().type == TokenType::EOF_TOKEN;
}

bool Parser::match(TokenType type)
{
    if (check(type))
    {
        advance();
        return true;
    }
    return false;
}

bool Parser::match(std::initializer_list<TokenType> types)
{
    for (TokenType type : types)
    {
        if (check(type))
        {
            advance();
            return true;
        }
    }
    return false;
}

Token Parser::consume(TokenType type, const std::string &message)
{
    if (check(type))
        return advance();
    throw error(peek(), message);
}

// Primary error function: formats and returns a ParseError.
ParseError Parser::error(const Token &token, const std::string &message)
{
    std::string errorMsg;
    if (token.type == TokenType::EOF_TOKEN)
    {
        errorMsg = token.filename + ":" + std::to_string(token.line) + ":" +
                   std::to_string(token.column) + " at end: " + message;
    }
    else
    {
        errorMsg = token.filename + ":" + std::to_string(token.line) + ":" +
                   std::to_string(token.column) + " at '" + token.value + "': " + message;
    }
    return ParseError(errorMsg);
}

void Parser::synchronize()
{
    advance();
    while (!isAtEnd())
    {
        if (previous().type == TokenType::NEWLINE)
            return;
        switch (peek().type)
        {
        case TokenType::DEF:
        case TokenType::CLASS:
        case TokenType::IF:
        case TokenType::FOR:
        case TokenType::WHILE:
        case TokenType::RETURN:
        case TokenType::IMPORT:
        case TokenType::MATCH:
            return;
        default:
            break;
        }
        advance();
    }
}

// ---------------------------------------------------------------------------
// Parsing Methods: Declarations, Statements, and Expressions
// ---------------------------------------------------------------------------

std::shared_ptr<ast::Statement> Parser::declaration()
{
    try
    {
        if (match(TokenType::LET) || match(TokenType::CONST))
        {
            return variableDeclaration();
        }
        if (match(TokenType::DEF))
        {
            return functionDeclaration();
        }
        if (match(TokenType::CLASS))
        {
            return classDeclaration();
        }
        return statement();
    }
    catch (const ParseError &error)
    {
        synchronize();
        throw error; // Re-throw to notify higher levels.
    }
}

std::shared_ptr<ast::VariableStmt> Parser::variableDeclaration()
{
    bool isConstant = previous().type == TokenType::CONST;
    Token name = consume(TokenType::IDENTIFIER, "Expect variable name.");

    std::shared_ptr<ast::Type> type = nullptr;
    if (match(TokenType::COLON))
    {
        type = parseType();
    }

    std::shared_ptr<ast::Expression> initializer = nullptr;
    if (match(TokenType::EQUAL))
    {
        initializer = expression();
    }
    else if (isConstant)
    {
        throw error(previous(), "Constant variables must be initialized.");
    }

    consume(TokenType::NEWLINE, "Expect newline after variable declaration.");
    return std::make_shared<ast::VariableStmt>(name, type, initializer, isConstant);
}

std::shared_ptr<ast::FunctionStmt> Parser::functionDeclaration()
{
    bool isAsync = false;
    bool isPure = false;

    if (match(TokenType::ASYNC))
    {
        isAsync = true;
    }

    if (match(TokenType::PURE))
    {
        isPure = true;
    }

    Token name = consume(TokenType::IDENTIFIER, "Expect function name.");
    consume(TokenType::LEFT_PAREN, "Expect '(' after function name.");

    std::vector<ast::Parameter> parameters;
    if (!check(TokenType::RIGHT_PAREN))
    {
        do
        {
            Token paramName = consume(TokenType::IDENTIFIER, "Expect parameter name.");
            consume(TokenType::COLON, "Expect ':' after parameter name.");
            std::shared_ptr<ast::Type> paramType = parseType();
            parameters.push_back(ast::Parameter(paramName.value, paramType));
        } while (match(TokenType::COMMA));
    }

    consume(TokenType::RIGHT_PAREN, "Expect ')' after parameters.");

    // Parse return type
    std::shared_ptr<ast::Type> returnType = nullptr;
    if (match(TokenType::ARROW))
    {
        returnType = parseType();
    }
    else
    {
        // Default to None (or void)
        Token voidToken(TokenType::IDENTIFIER, "None", name.filename, name.line, name.column);
        returnType = std::make_shared<ast::SimpleType>(voidToken);
    }

    consume(TokenType::COLON, "Expect ':' before function body.");
    consume(TokenType::NEWLINE, "Expect newline after function declaration.");

    std::shared_ptr<ast::BlockStmt> body = block();

    return std::make_shared<ast::FunctionStmt>(name, parameters, returnType, body, isAsync, isPure);
}

std::shared_ptr<ast::ClassStmt> Parser::classDeclaration()
{
    Token name = consume(TokenType::IDENTIFIER, "Expect class name.");

    std::vector<std::shared_ptr<ast::Type>> superclasses;
    if (match(TokenType::LEFT_PAREN))
    {
        do
        {
            superclasses.push_back(parseType());
        } while (match(TokenType::COMMA));
        consume(TokenType::RIGHT_PAREN, "Expect ')' after superclasses.");
    }

    consume(TokenType::COLON, "Expect ':' before class body.");
    consume(TokenType::NEWLINE, "Expect newline after class declaration.");
    consume(TokenType::INDENT, "Expect indented class body.");

    std::vector<std::shared_ptr<ast::VariableStmt>> fields;
    std::vector<std::shared_ptr<ast::FunctionStmt>> methods;

    while (!check(TokenType::DEDENT) && !isAtEnd())
    {
        if (match(TokenType::LET) || match(TokenType::CONST))
        {
            fields.push_back(variableDeclaration());
        }
        else if (match(TokenType::DEF))
        {
            methods.push_back(functionDeclaration());
        }
        else
        {
            throw error(peek(), "Expect field or method declaration in class body.");
        }
    }

    consume(TokenType::DEDENT, "Expect dedent after class body.");

    return std::make_shared<ast::ClassStmt>(name, superclasses, fields, methods);
}

std::shared_ptr<ast::Statement> Parser::statement()
{
    if (match(TokenType::IF))
        return ifStatement();
    if (match(TokenType::WHILE))
        return whileStatement();
    if (match(TokenType::FOR))
        return forStatement();
    if (match(TokenType::RETURN))
        return returnStatement();
    if (match(TokenType::IMPORT) || match(TokenType::FROM))
        return importStatement();
    if (match(TokenType::MATCH))
        return matchStatement();
    if (match(TokenType::INDENT))
    {
        return block();
    }

    return expressionStatement();
}

std::shared_ptr<ast::ExpressionStmt> Parser::expressionStatement()
{
    Token token = peek();
    std::shared_ptr<ast::Expression> expr = expression();
    consume(TokenType::NEWLINE, "Expect newline after expression.");
    return std::make_shared<ast::ExpressionStmt>(token, expr);
}

std::shared_ptr<ast::BlockStmt> Parser::block()
{
    Token token = previous(); // INDENT token
    std::vector<std::shared_ptr<ast::Statement>> statements;

    while (!check(TokenType::DEDENT) && !isAtEnd())
    {
        statements.push_back(declaration());
    }

    consume(TokenType::DEDENT, "Expect dedent after block.");

    return std::make_shared<ast::BlockStmt>(token, statements);
}

std::shared_ptr<ast::IfStmt> Parser::ifStatement()
{
    Token token = previous(); // IF token
    std::shared_ptr<ast::Expression> condition = expression();
    consume(TokenType::COLON, "Expect ':' after if condition.");
    consume(TokenType::NEWLINE, "Expect newline after if declaration.");

    std::shared_ptr<ast::Statement> thenBranch = block();
    std::vector<std::pair<std::shared_ptr<ast::Expression>, std::shared_ptr<ast::Statement>>> elifBranches;
    std::shared_ptr<ast::Statement> elseBranch = nullptr;

    while (match(TokenType::ELIF))
    {
        std::shared_ptr<ast::Expression> elifCondition = expression();
        consume(TokenType::COLON, "Expect ':' after elif condition.");
        consume(TokenType::NEWLINE, "Expect newline after elif declaration.");

        std::shared_ptr<ast::Statement> elifBranch = block();
        elifBranches.push_back(std::make_pair(elifCondition, elifBranch));
    }

    if (match(TokenType::ELSE))
    {
        consume(TokenType::COLON, "Expect ':' after else.");
        consume(TokenType::NEWLINE, "Expect newline after else declaration.");
        elseBranch = block();
    }

    return std::make_shared<ast::IfStmt>(token, condition, thenBranch, elifBranches, elseBranch);
}

std::shared_ptr<ast::WhileStmt> Parser::whileStatement()
{
    Token token = previous(); // WHILE token
    std::shared_ptr<ast::Expression> condition = expression();
    consume(TokenType::COLON, "Expect ':' after while condition.");
    consume(TokenType::NEWLINE, "Expect newline after while declaration.");

    std::shared_ptr<ast::Statement> body = block();

    return std::make_shared<ast::WhileStmt>(token, condition, body);
}

std::shared_ptr<ast::ForStmt> Parser::forStatement()
{
    Token token = previous(); // FOR token
    Token variableToken = consume(TokenType::IDENTIFIER, "Expect variable name in for statement.");
    std::string variable = variableToken.value;

    std::shared_ptr<ast::Type> variableType = nullptr;
    if (match(TokenType::COLON))
    {
        variableType = parseType();
    }

    consume(TokenType::IN, "Expect 'in' after for variable.");

    std::shared_ptr<ast::Expression> iterable = expression();
    consume(TokenType::COLON, "Expect ':' after for iterable.");
    consume(TokenType::NEWLINE, "Expect newline after for declaration.");

    std::shared_ptr<ast::Statement> body = block();

    return std::make_shared<ast::ForStmt>(token, variable, variableType, iterable, body);
}

std::shared_ptr<ast::ReturnStmt> Parser::returnStatement()
{
    Token token = previous(); // RETURN token

    std::shared_ptr<ast::Expression> value = nullptr;
    if (!check(TokenType::NEWLINE))
    {
        value = expression();
    }

    consume(TokenType::NEWLINE, "Expect newline after return statement.");

    return std::make_shared<ast::ReturnStmt>(token, value);
}

std::shared_ptr<ast::ImportStmt> Parser::importStatement()
{
    Token token = previous(); // IMPORT or FROM token
    std::string module;
    std::vector<std::pair<std::string, std::string>> imports;

    if (token.type == TokenType::FROM)
    {
        // from X import Y [as Z], ...
        Token moduleToken = consume(TokenType::IDENTIFIER, "Expect module name after 'from'.");
        module = moduleToken.value;

        consume(TokenType::IMPORT, "Expect 'import' after module name.");

        do
        {
            Token importName = consume(TokenType::IDENTIFIER, "Expect import name.");
            std::string alias = importName.value;

            if (match(TokenType::IDENTIFIER))
            {
                if (previous().value != "as")
                {
                    throw error(previous(), "Expect 'as' for import alias.");
                }

                Token aliasToken = consume(TokenType::IDENTIFIER, "Expect alias name after 'as'.");
                alias = aliasToken.value;
            }

            imports.push_back(std::make_pair(importName.value, alias));
        } while (match(TokenType::COMMA));
    }
    else
    {
        // import X [as Y]
        Token moduleToken = consume(TokenType::IDENTIFIER, "Expect module name after 'import'.");
        module = moduleToken.value;

        std::string alias = module;
        if (match(TokenType::IDENTIFIER))
        {
            if (previous().value != "as")
            {
                throw error(previous(), "Expect 'as' for import alias.");
            }

            Token aliasToken = consume(TokenType::IDENTIFIER, "Expect alias name after 'as'.");
            alias = aliasToken.value;
        }

        imports.push_back(std::make_pair(module, alias));
    }

    consume(TokenType::NEWLINE, "Expect newline after import statement.");

    return std::make_shared<ast::ImportStmt>(token, module, imports);
}

std::shared_ptr<ast::MatchStmt> Parser::matchStatement()
{
    Token token = previous(); // MATCH token
    std::shared_ptr<ast::Expression> value = expression();
    consume(TokenType::COLON, "Expect ':' after match value.");
    consume(TokenType::NEWLINE, "Expect newline after match declaration.");
    consume(TokenType::INDENT, "Expect indented match body.");

    std::vector<ast::MatchCase> cases;
    std::shared_ptr<ast::Statement> defaultCase = nullptr;

    while (!check(TokenType::DEDENT) && !isAtEnd())
    {
        if (match(TokenType::CASE))
        {
            std::shared_ptr<ast::Expression> pattern = expression();
            consume(TokenType::COLON, "Expect ':' after case pattern.");
            consume(TokenType::NEWLINE, "Expect newline after case declaration.");

            std::shared_ptr<ast::Statement> body = block();
            cases.push_back(ast::MatchCase(pattern, body));
        }
        else if (match(TokenType::DEFAULT))
        {
            consume(TokenType::COLON, "Expect ':' after default.");
            consume(TokenType::NEWLINE, "Expect newline after default declaration.");

            defaultCase = block();
        }
        else
        {
            throw error(peek(), "Expect 'case' or 'default' in match statement.");
        }
    }

    consume(TokenType::DEDENT, "Expect dedent after match body.");

    return std::make_shared<ast::MatchStmt>(token, value, cases, defaultCase);
}

// ---------------------------------------------------------------------------
// Expression Parsing
// ---------------------------------------------------------------------------

std::shared_ptr<ast::Expression> Parser::expression()
{
    return assignment();
}

std::shared_ptr<ast::Expression> Parser::assignment()
{
    std::shared_ptr<ast::Expression> expr = logicalOr();

    if (match(TokenType::EQUAL))
    {
        Token equals = previous();
        std::shared_ptr<ast::Expression> value = assignment();

        if (auto varExpr = std::dynamic_pointer_cast<ast::VariableExpr>(expr))
        {
            return std::make_shared<ast::AssignExpr>(varExpr->token, value);
        }
        else if (auto getExpr = std::dynamic_pointer_cast<ast::GetExpr>(expr))
        {
            return std::make_shared<ast::SetExpr>(getExpr->token, getExpr->object, value);
        }

        throw error(equals, "Invalid assignment target.");
    }

    return expr;
}

std::shared_ptr<ast::Expression> Parser::logicalOr()
{
    std::shared_ptr<ast::Expression> expr = logicalAnd();

    while (match(TokenType::OR))
    {
        Token op = previous();
        std::shared_ptr<ast::Expression> right = logicalAnd();
        expr = std::make_shared<ast::BinaryExpr>(expr, op, right);
    }

    return expr;
}

std::shared_ptr<ast::Expression> Parser::logicalAnd()
{
    std::shared_ptr<ast::Expression> expr = equality();

    while (match(TokenType::AND))
    {
        Token op = previous();
        std::shared_ptr<ast::Expression> right = equality();
        expr = std::make_shared<ast::BinaryExpr>(expr, op, right);
    }

    return expr;
}

std::shared_ptr<ast::Expression> Parser::equality()
{
    std::shared_ptr<ast::Expression> expr = comparison();

    while (match({TokenType::EQUAL_EQUAL, TokenType::BANG_EQUAL}))
    {
        Token op = previous();
        std::shared_ptr<ast::Expression> right = comparison();
        expr = std::make_shared<ast::BinaryExpr>(expr, op, right);
    }

    return expr;
}

std::shared_ptr<ast::Expression> Parser::comparison()
{
    std::shared_ptr<ast::Expression> expr = addition();

    while (match({TokenType::GREATER, TokenType::GREATER_EQUAL, TokenType::LESS, TokenType::LESS_EQUAL}))
    {
        Token op = previous();
        std::shared_ptr<ast::Expression> right = addition();
        expr = std::make_shared<ast::BinaryExpr>(expr, op, right);
    }

    return expr;
}

std::shared_ptr<ast::Expression> Parser::addition()
{
    std::shared_ptr<ast::Expression> expr = multiplication();

    while (match({TokenType::PLUS, TokenType::MINUS}))
    {
        Token op = previous();
        std::shared_ptr<ast::Expression> right = multiplication();
        expr = std::make_shared<ast::BinaryExpr>(expr, op, right);
    }

    return expr;
}

std::shared_ptr<ast::Expression> Parser::multiplication()
{
    std::shared_ptr<ast::Expression> expr = unary();

    while (match({TokenType::STAR, TokenType::SLASH, TokenType::PERCENT}))
    {
        Token op = previous();
        std::shared_ptr<ast::Expression> right = unary();
        expr = std::make_shared<ast::BinaryExpr>(expr, op, right);
    }

    return expr;
}

std::shared_ptr<ast::Expression> Parser::unary()
{
    if (match({TokenType::BANG, TokenType::MINUS}))
    {
        Token op = previous();
        std::shared_ptr<ast::Expression> right = unary();
        return std::make_shared<ast::UnaryExpr>(op, right);
    }

    return call();
}

std::shared_ptr<ast::Expression> Parser::call()
{
    std::shared_ptr<ast::Expression> expr = primary();

    while (true)
    {
        if (match(TokenType::LEFT_PAREN))
        {
            expr = finishCall(expr);
        }
        else if (match(TokenType::DOT))
        {
            Token name = consume(TokenType::IDENTIFIER, "Expect property name after '.'.");
            expr = std::make_shared<ast::GetExpr>(name, expr);
        }
        else if (match(TokenType::LEFT_BRACKET))
        {
            // Array indexing syntax: list[index]
            Token bracketToken = previous();
            std::shared_ptr<ast::Expression> index = expression();
            consume(TokenType::RIGHT_BRACKET, "Expect ']' after index.");
            Token getItemToken(TokenType::IDENTIFIER, "__getitem__", bracketToken.filename, bracketToken.line, bracketToken.column);
            std::shared_ptr<ast::GetExpr> getItemExpr = std::make_shared<ast::GetExpr>(getItemToken, expr);
            std::vector<std::shared_ptr<ast::Expression>> args = {index};
            expr = std::make_shared<ast::CallExpr>(getItemToken, getItemExpr, args);
        }
        else
        {
            break;
        }
    }

    return expr;
}

std::shared_ptr<ast::Expression> Parser::finishCall(std::shared_ptr<ast::Expression> callee)
{
    std::vector<std::shared_ptr<ast::Expression>> arguments;

    if (!check(TokenType::RIGHT_PAREN))
    {
        do
        {
            if (arguments.size() >= 255)
            {
                throw error(peek(), "Cannot have more than 255 arguments.");
            }
            arguments.push_back(expression());
        } while (match(TokenType::COMMA));
    }

    Token paren = consume(TokenType::RIGHT_PAREN, "Expect ')' after arguments.");
    return std::make_shared<ast::CallExpr>(paren, callee, arguments);
}

std::shared_ptr<ast::Expression> Parser::primary()
{
    if (match(TokenType::FALSE))
    {
        return std::make_shared<ast::LiteralExpr>(previous(), ast::LiteralExpr::LiteralType::BOOLEAN, "False");
    }
    if (match(TokenType::TRUE))
    {
        return std::make_shared<ast::LiteralExpr>(previous(), ast::LiteralExpr::LiteralType::BOOLEAN, "True");
    }
    if (match(TokenType::NIL))
    {
        return std::make_shared<ast::LiteralExpr>(previous(), ast::LiteralExpr::LiteralType::NIL, "None");
    }
    if (match(TokenType::INTEGER_LITERAL))
    {
        return std::make_shared<ast::LiteralExpr>(previous(), ast::LiteralExpr::LiteralType::INTEGER, previous().value);
    }
    if (match(TokenType::FLOAT_LITERAL))
    {
        return std::make_shared<ast::LiteralExpr>(previous(), ast::LiteralExpr::LiteralType::FLOAT, previous().value);
    }
    if (match(TokenType::STRING_LITERAL))
    {
        return std::make_shared<ast::LiteralExpr>(previous(), ast::LiteralExpr::LiteralType::STRING, previous().value);
    }
    if (match(TokenType::IDENTIFIER))
    {
        return std::make_shared<ast::VariableExpr>(previous());
    }
    if (match(TokenType::LEFT_PAREN))
    {
        Token token = previous();
        std::shared_ptr<ast::Expression> expr = expression();
        consume(TokenType::RIGHT_PAREN, "Expect ')' after expression.");
        return std::make_shared<ast::GroupingExpr>(token, expr);
    }
    if (match(TokenType::LEFT_BRACKET))
    {
        return list();
    }
    if (match(TokenType::LEFT_BRACE))
    {
        return dictionary();
    }
    if (match(TokenType::DEF))
    {
        return lambda();
    }
    throw error(peek(), "Expect expression.");
}

std::shared_ptr<ast::Expression> Parser::list()
{
    Token token = previous(); // '[' token
    std::vector<std::shared_ptr<ast::Expression>> elements;

    if (!check(TokenType::RIGHT_BRACKET))
    {
        do
        {
            elements.push_back(expression());
        } while (match(TokenType::COMMA));
    }

    consume(TokenType::RIGHT_BRACKET, "Expect ']' after list elements.");
    return std::make_shared<ast::ListExpr>(token, elements);
}

std::shared_ptr<ast::Expression> Parser::dictionary()
{
    Token token = previous(); // '{' token
    std::vector<std::pair<std::shared_ptr<ast::Expression>, std::shared_ptr<ast::Expression>>> entries;

    if (!check(TokenType::RIGHT_BRACE))
    {
        do
        {
            std::shared_ptr<ast::Expression> key = expression();
            consume(TokenType::COLON, "Expect ':' after dictionary key.");
            std::shared_ptr<ast::Expression> value = expression();
            entries.push_back(std::make_pair(key, value));
        } while (match(TokenType::COMMA));
    }

    consume(TokenType::RIGHT_BRACE, "Expect '}' after dictionary entries.");
    return std::make_shared<ast::DictionaryExpr>(token, entries);
}

std::shared_ptr<ast::Expression> Parser::lambda()
{
    Token token = previous(); // 'def' token
    consume(TokenType::LEFT_PAREN, "Expect '(' after 'def' in lambda expression.");

    std::vector<ast::Parameter> parameters;
    if (!check(TokenType::RIGHT_PAREN))
    {
        do
        {
            Token paramName = consume(TokenType::IDENTIFIER, "Expect parameter name.");
            consume(TokenType::COLON, "Expect ':' after parameter name.");
            std::shared_ptr<ast::Type> paramType = parseType();
            parameters.push_back(ast::Parameter(paramName.value, paramType));
        } while (match(TokenType::COMMA));
    }

    consume(TokenType::RIGHT_PAREN, "Expect ')' after lambda parameters.");

    std::shared_ptr<ast::Type> returnType = nullptr;
    if (match(TokenType::ARROW))
    {
        returnType = parseType();
    }
    else
    {
        Token voidToken(TokenType::IDENTIFIER, "None", token.filename, token.line, token.column);
        returnType = std::make_shared<ast::SimpleType>(voidToken);
    }

    consume(TokenType::COLON, "Expect ':' before lambda body.");

    // Lambda body: either a block or a single expression.
    std::shared_ptr<ast::Statement> body;
    if (match(TokenType::NEWLINE))
    {
        body = block();
    }
    else
    {
        Token exprToken = peek();
        std::shared_ptr<ast::Expression> expr = expression();
        body = std::make_shared<ast::ExpressionStmt>(exprToken, expr);
    }

    return std::make_shared<ast::LambdaExpr>(token, parameters, returnType, body);

    if (match(TokenType::NEWLINE) || match(TokenType::SEMICOLON))
    {
        advance(); // Move past the newline or semicolon
    }
    else
    {
        throw error(peek(), "Expect newline or semicolon after expression.");
    }
}

// ---------------------------------------------------------------------------
// Type Parsing
// ---------------------------------------------------------------------------

std::shared_ptr<ast::Type> Parser::parseType()
{
    if (match(TokenType::IDENTIFIER))
    {
        Token nameToken = previous();
        if (match(TokenType::LEFT_BRACKET))
        {
            return parseGenericType(nameToken);
        }
        return std::make_shared<ast::SimpleType>(nameToken);
    }
    throw error(peek(), "Expect type name.");
}

std::shared_ptr<ast::Type> Parser::parseGenericType(const Token &nameToken)
{
    std::vector<std::shared_ptr<ast::Type>> typeArguments;

    if (!check(TokenType::RIGHT_BRACKET))
    {
        do
        {
            typeArguments.push_back(parseType());
        } while (match(TokenType::COMMA));
    }

    consume(TokenType::RIGHT_BRACKET, "Expect ']' after type arguments.");
    return std::make_shared<ast::GenericType>(nameToken, nameToken.value, typeArguments);
}
