#include "parser.h"
#include <stdexcept>
#include <iostream>

namespace parser
{

    Parser::Parser(const std::vector<lexer::Token> &tokens)
        : tokens(tokens), current(0) {
        errors_.clear();
    }

    ast::StmtPtr Parser::parse()
    {
        try
        {
            std::vector<ast::StmtPtr> statements;

            while (!isAtEnd())
            {
                ast::StmtPtr stmt = declaration();
                if (stmt)
                {
                    statements.push_back(stmt);
                }
            }

            // If we only have one statement, return it directly
            if (statements.size() == 1)
            {
                return statements[0];
            }

            // Otherwise, create a block statement
            return std::make_shared<ast::BlockStmt>(
                tokens.empty() ? lexer::Token(lexer::TokenType::EOF_TOKEN, "", "", 0, 0) : tokens[0],
                std::move(statements));
        }
        catch (const std::exception &e)
        {
            errorHandler.reportError(error::ErrorCode::S004_INVALID_STATEMENT,
                                     "Parser exception: " + std::string(e.what()),
                                     "", 0, 0, error::ErrorSeverity::FATAL);
            return nullptr;
        }
    }

    ast::StmtPtr Parser::declaration()
    {
        try
        {
            if (match(lexer::TokenType::LET) || match(lexer::TokenType::CONST))
            {
                return varDeclaration();
            }
            if (match(lexer::TokenType::DEF) || match(lexer::TokenType::ASYNC))
            {
                return functionDeclaration();
            }
            if (match(lexer::TokenType::EXTERN))
            {
                // extern def f(params) -> T;  -> external (C) function declaration.
                consume(lexer::TokenType::DEF, "Expected 'def' after 'extern'");
                return methodDeclaration(/*allowNoBody=*/true);
            }
            if (match(lexer::TokenType::CLASS) || match(lexer::TokenType::STRUCT))
            {
                return classDeclaration();
            }
            if (match(lexer::TokenType::ENUM))
            {
                return enumDeclaration();
            }
            if (match(lexer::TokenType::TRAIT))
            {
                return traitDeclaration();
            }
            if (match(lexer::TokenType::IMPL))
            {
                return implDeclaration();
            }
            if (match(lexer::TokenType::IMPORT))
            {
                return importStmt();
            }
            return statement();
        }
        catch (const std::exception &e)
        {
            synchronize();
            errorHandler.reportError(error::ErrorCode::S004_INVALID_STATEMENT,
                                     "Parser exception in declaration: " + std::string(e.what()),
                                     peek(),
                                     error::ErrorSeverity::ERROR);
            return nullptr;
        }
    }

    ast::StmtPtr Parser::varDeclaration()
    {
        bool isConstant = previous().type == lexer::TokenType::CONST;
        auto name = consume(lexer::TokenType::IDENTIFIER, "Expected variable name");
        ast::TypePtr type = nullptr;
        if (match(lexer::TokenType::COLON))
        {
            type = parseType();
        }
        ast::ExprPtr initializer = nullptr;
        if (match(lexer::TokenType::EQUAL))
        {
            initializer = expression();
        }
        consume(lexer::TokenType::SEMI_COLON, "Expected ';' after variable declaration");
        return std::make_shared<ast::VariableStmt>(name, name.value, type, initializer, isConstant);
    }

    std::vector<ast::TypeParameter> Parser::parseTypeParameters()
    {
        std::vector<ast::TypeParameter> typeParams;
        if (match(lexer::TokenType::LESS))
        {
            do
            {
                auto tpName = consume(lexer::TokenType::IDENTIFIER, "Expected type parameter name");
                // Optional constraint `: SomeTrait` is parsed and ignored for now.
                if (match(lexer::TokenType::COLON))
                    parseType();
                typeParams.emplace_back(tpName, tpName.value);
            } while (match(lexer::TokenType::COMMA));
            consume(lexer::TokenType::GREATER, "Expected '>' after type parameters");
        }
        return typeParams;
    }

    ast::StmtPtr Parser::functionDeclaration()
    {
        bool isAsync = previous().type == lexer::TokenType::ASYNC;
        if (isAsync && !match(lexer::TokenType::DEF))
        {
            error(previous(), "Expected 'def' after 'async'");
        }
        auto name = consume(lexer::TokenType::IDENTIFIER, "Expected function name");
        // Optional generic type parameters: def f<T, U>(...)
        std::vector<ast::TypeParameter> typeParams = parseTypeParameters();
        consume(lexer::TokenType::LEFT_PAREN, "Expected '(' after function name");
        auto parameters = parseParameters();
        consume(lexer::TokenType::RIGHT_PAREN, "Expected ')' after parameters");
        ast::TypePtr returnType = nullptr;
        // Support both -> and : for return type annotation. When neither is
        // present, leave the return type unspecified (null) so it can be
        // inferred from the function body during type checking / codegen.
        if (match(lexer::TokenType::ARROW) || match(lexer::TokenType::COLON))
        {
            returnType = parseType();
        }
        consume(lexer::TokenType::LEFT_BRACE, "Expected '{' before function body");
        auto body = blockStmt();
        if (!typeParams.empty())
            return std::make_shared<ast::FunctionStmt>(name, name.value, typeParams,
                                                       parameters, returnType, body, isAsync);
        return std::make_shared<ast::FunctionStmt>(name, name.value, parameters, returnType, body, isAsync);
    }

    ast::StmtPtr Parser::classDeclaration()
    {
        auto name = consume(lexer::TokenType::IDENTIFIER, "Expected class name");
        std::vector<ast::TypeParameter> typeParams = parseTypeParameters();
        consume(lexer::TokenType::LEFT_BRACE, "Expected '{' before class body");
        std::vector<ast::StmtPtr> fields, methods;
        while (!check(lexer::TokenType::RIGHT_BRACE) && !isAtEnd())
        {
            if (match(lexer::TokenType::LET) || match(lexer::TokenType::CONST))
            {
                fields.push_back(varDeclaration());
            }
            else if (match(lexer::TokenType::DEF) || match(lexer::TokenType::ASYNC))
            {
                methods.push_back(functionDeclaration());
            }
            else if (check(lexer::TokenType::IDENTIFIER))
            {
                // Bare field declaration: `name: Type;` (struct-style)
                auto fieldName = consume(lexer::TokenType::IDENTIFIER, "Expected field name");
                ast::TypePtr fieldType = nullptr;
                if (match(lexer::TokenType::COLON))
                    fieldType = parseType();
                ast::ExprPtr init = nullptr;
                if (match(lexer::TokenType::EQUAL))
                    init = expression();
                match(lexer::TokenType::SEMI_COLON); // optional trailing ';'
                fields.push_back(std::make_shared<ast::VariableStmt>(
                    fieldName, fieldName.value, fieldType, init, false));
            }
            else
            {
                error(peek(), "Expected field or method declaration");
                advance();
            }
        }
        consume(lexer::TokenType::RIGHT_BRACE, "Expected '}' after class body");
        if (!typeParams.empty())
            return std::make_shared<ast::ClassStmt>(name, name.value, typeParams,
                                                    nullptr, std::vector<ast::TypePtr>{},
                                                    fields, methods);
        return std::make_shared<ast::ClassStmt>(name, name.value, fields, methods);
    }

    std::shared_ptr<ast::FunctionStmt> Parser::methodDeclaration(bool allowNoBody)
    {
        bool isAsync = previous().type == lexer::TokenType::ASYNC;
        auto name = consume(lexer::TokenType::IDENTIFIER, "Expected method name");
        std::vector<ast::TypeParameter> typeParams = parseTypeParameters();
        consume(lexer::TokenType::LEFT_PAREN, "Expected '(' after method name");
        auto parameters = parseParameters();
        consume(lexer::TokenType::RIGHT_PAREN, "Expected ')' after parameters");
        ast::TypePtr returnType = nullptr;
        if (match(lexer::TokenType::ARROW) || match(lexer::TokenType::COLON))
            returnType = parseType();
        ast::StmtPtr body = nullptr;
        if (match(lexer::TokenType::LEFT_BRACE))
            body = blockStmt();
        else if (allowNoBody)
            match(lexer::TokenType::SEMI_COLON); // signature only
        else
        {
            consume(lexer::TokenType::LEFT_BRACE, "Expected '{' before method body");
            body = blockStmt();
        }
        if (!typeParams.empty())
            return std::make_shared<ast::FunctionStmt>(name, name.value, typeParams,
                                                       parameters, returnType, body, isAsync);
        return std::make_shared<ast::FunctionStmt>(name, name.value, parameters, returnType, body, isAsync);
    }

    ast::StmtPtr Parser::enumDeclaration()
    {
        auto name = consume(lexer::TokenType::IDENTIFIER, "Expected enum name");
        consume(lexer::TokenType::LEFT_BRACE, "Expected '{' before enum body");
        std::vector<std::pair<std::string, int64_t>> members;
        int64_t next = 0;
        while (!check(lexer::TokenType::RIGHT_BRACE) && !isAtEnd())
        {
            auto member = consume(lexer::TokenType::IDENTIFIER, "Expected enum member name");
            int64_t value = next;
            if (match(lexer::TokenType::EQUAL))
            {
                bool neg = match(lexer::TokenType::MINUS);
                auto v = consume(lexer::TokenType::INT, "Expected integer after '=' in enum");
                value = std::stoll(v.value);
                if (neg) value = -value;
            }
            members.emplace_back(member.value, value);
            next = value + 1;
            match(lexer::TokenType::COMMA); // optional separator
        }
        consume(lexer::TokenType::RIGHT_BRACE, "Expected '}' after enum body");
        return std::make_shared<ast::EnumStmt>(name, name.value, members);
    }

    ast::StmtPtr Parser::traitDeclaration()
    {
        auto name = consume(lexer::TokenType::IDENTIFIER, "Expected trait name");
        parseTypeParameters(); // optional generic params (ignored for now)
        consume(lexer::TokenType::LEFT_BRACE, "Expected '{' before trait body");
        auto trait = std::make_shared<ast::TraitStmt>(name, name.value);
        while (!check(lexer::TokenType::RIGHT_BRACE) && !isAtEnd())
        {
            if (match(lexer::TokenType::DEF) || match(lexer::TokenType::ASYNC))
                trait->methods.push_back(methodDeclaration(/*allowNoBody=*/true));
            else
            {
                error(peek(), "Expected method declaration in trait");
                advance();
            }
        }
        consume(lexer::TokenType::RIGHT_BRACE, "Expected '}' after trait body");
        return trait;
    }

    ast::StmtPtr Parser::implDeclaration()
    {
        auto first = consume(lexer::TokenType::IDENTIFIER, "Expected type or trait name after 'impl'");
        std::string traitName;
        lexer::Token typeTok = first;
        if (match(lexer::TokenType::FOR))
        {
            traitName = first.value;
            typeTok = consume(lexer::TokenType::IDENTIFIER, "Expected type name after 'for'");
        }
        parseTypeParameters(); // optional generic params (ignored for now)
        ast::TypePtr typePtr = std::make_shared<ast::SimpleType>(
            lexer::Token(lexer::TokenType::IDENTIFIER, typeTok.value, "", typeTok.line, typeTok.column));
        consume(lexer::TokenType::LEFT_BRACE, "Expected '{' before impl body");
        auto impl = std::make_shared<ast::ImplStmt>(first, traitName, typePtr);
        while (!check(lexer::TokenType::RIGHT_BRACE) && !isAtEnd())
        {
            if (match(lexer::TokenType::DEF) || match(lexer::TokenType::ASYNC))
                impl->methods.push_back(methodDeclaration(/*allowNoBody=*/false));
            else
            {
                error(peek(), "Expected method declaration in impl");
                advance();
            }
        }
        consume(lexer::TokenType::RIGHT_BRACE, "Expected '}' after impl body");
        return impl;
    }

    ast::StmtPtr Parser::statement()
    {
        if (match(lexer::TokenType::IF))
            return ifStmt();
        if (match(lexer::TokenType::WHILE))
            return whileStmt();
        if (match(lexer::TokenType::FOR))
            return forStmt();
        if (match(lexer::TokenType::LEFT_BRACE))
            return blockStmt();
        if (match(lexer::TokenType::RETURN))
            return returnStmt();
        if (match(lexer::TokenType::MATCH))
            return matchStmt();
        if (match(lexer::TokenType::GO))
            return goStmt();
        if (match(lexer::TokenType::SELECT))
            return selectStmt();
        if (match(lexer::TokenType::TRY))
            return tryStmt();
        if (match(lexer::TokenType::THROW))
            return throwStmt();
        return expressionStmt();
    }

    // try { ... } catch (e) { ... } finally { ... }
    // The catch clause and finally clause are each optional, but at least one
    // must be present. The catch variable is optional: `catch { ... }`.
    ast::StmtPtr Parser::tryStmt()
    {
        lexer::Token keyword = previous();
        consume(lexer::TokenType::LEFT_BRACE, "Expected '{' after 'try'");
        ast::StmtPtr tryBlock = blockStmt();

        std::string catchVar;
        ast::StmtPtr catchBlock = nullptr;
        if (match(lexer::TokenType::CATCH))
        {
            // Optional `(name)` or bare `name` binding for the caught value.
            if (match(lexer::TokenType::LEFT_PAREN))
            {
                if (check(lexer::TokenType::IDENTIFIER))
                    catchVar = advance().value;
                consume(lexer::TokenType::RIGHT_PAREN, "Expected ')' after catch variable");
            }
            else if (check(lexer::TokenType::IDENTIFIER))
            {
                catchVar = advance().value;
            }
            consume(lexer::TokenType::LEFT_BRACE, "Expected '{' after catch");
            catchBlock = blockStmt();
        }

        ast::StmtPtr finallyBlock = nullptr;
        if (match(lexer::TokenType::FINALLY))
        {
            consume(lexer::TokenType::LEFT_BRACE, "Expected '{' after finally");
            finallyBlock = blockStmt();
        }

        if (!catchBlock && !finallyBlock)
            error(keyword, "'try' must be followed by 'catch' or 'finally'");

        return std::make_shared<ast::TryStmt>(keyword, tryBlock, catchVar,
                                              catchBlock, finallyBlock);
    }

    // throw <expr>;
    ast::StmtPtr Parser::throwStmt()
    {
        lexer::Token keyword = previous();
        ast::ExprPtr value = expression();
        consume(lexer::TokenType::SEMI_COLON, "Expected ';' after thrown value");
        return std::make_shared<ast::ThrowStmt>(keyword, value);
    }

    ast::StmtPtr Parser::expressionStmt()
    {
        auto expr = expression();
        consume(lexer::TokenType::SEMI_COLON, "Expected ';' after expression");
        return std::make_shared<ast::ExpressionStmt>(expr->token, expr);
    }

    ast::StmtPtr Parser::ifStmt()
    {
        auto condition = expression();
        consume(lexer::TokenType::LEFT_BRACE, "Expected '{' after if condition");
        auto thenBranch = blockStmt();
        std::vector<std::pair<ast::ExprPtr, ast::StmtPtr>> elifBranches;
        while (match(lexer::TokenType::ELIF))
        {
            auto elifCondition = expression();
            consume(lexer::TokenType::LEFT_BRACE, "Expected '{' after elif condition");
            auto elifBranch = blockStmt();
            elifBranches.emplace_back(elifCondition, elifBranch);
        }
        ast::StmtPtr elseBranch = nullptr;
        if (match(lexer::TokenType::ELSE))
        {
            consume(lexer::TokenType::LEFT_BRACE, "Expected '{' after else");
            elseBranch = blockStmt();
        }
        return std::make_shared<ast::IfStmt>(condition->token, condition, thenBranch, elifBranches, elseBranch);
    }

    ast::StmtPtr Parser::whileStmt()
    {
        auto condition = expression();
        consume(lexer::TokenType::LEFT_BRACE, "Expected '{' after while condition");
        auto body = blockStmt();
        return std::make_shared<ast::WhileStmt>(condition->token, condition, body);
    }

    ast::StmtPtr Parser::forStmt()
    {
        auto variable = consume(lexer::TokenType::IDENTIFIER, "Expected loop variable");
        ast::TypePtr variableType = nullptr;
        if (match(lexer::TokenType::COLON))
        {
            variableType = parseType();
        }
        consume(lexer::TokenType::IN, "Expected 'in' after loop variable");
        auto iterable = expression();
        // Range syntax: `for i in start..end` builds a RANGE binary expression
        // that codegen lowers into a counting loop.
        if (match(lexer::TokenType::RANGE))
        {
            lexer::Token rangeOp = previous();
            auto end = expression();
            iterable = std::make_shared<ast::BinaryExpr>(iterable->token, iterable, rangeOp, end);
        }
        consume(lexer::TokenType::LEFT_BRACE, "Expected '{' after for iterable");
        auto body = blockStmt();
        return std::make_shared<ast::ForStmt>(variable, variable.value, variableType, iterable, body);
    }

    ast::StmtPtr Parser::blockStmt()
    {
        std::vector<ast::StmtPtr> statements;
        while (!check(lexer::TokenType::RIGHT_BRACE) && !isAtEnd())
        {
            statements.push_back(declaration());
        }
        consume(lexer::TokenType::RIGHT_BRACE, "Expected '}' after block");
        return std::make_shared<ast::BlockStmt>(previous(), statements);
    }

    ast::StmtPtr Parser::returnStmt()
    {
        auto keyword = previous();
        ast::ExprPtr value = nullptr;
        if (!check(lexer::TokenType::SEMI_COLON))
        {
            value = expression();
        }
        consume(lexer::TokenType::SEMI_COLON, "Expected ';' after return value");
        return std::make_shared<ast::ReturnStmt>(keyword, value);
    }

    ast::StmtPtr Parser::importStmt()
    {
        lexer::Token tok = peek();
        std::string path;
        if (match(lexer::TokenType::STRING))
        {
            // import "relative/or/std/path"
            path = previous().value;
            if (path.size() >= 2 && (path.front() == '"' || path.front() == '\''))
                path = path.substr(1, path.size() - 2);
        }
        else
        {
            // import a.b.c  ->  a/b/c
            auto first = consume(lexer::TokenType::IDENTIFIER, "Expected module name");
            path = first.value;
            while (match(lexer::TokenType::DOT))
            {
                auto part = consume(lexer::TokenType::IDENTIFIER, "Expected name after '.'");
                path += "/" + part.value;
            }
        }
        match(lexer::TokenType::SEMI_COLON); // optional ';'
        return std::make_shared<ast::ImportStmt>(tok, path);
    }

    ast::StmtPtr Parser::matchStmt()
    {
        auto value = expression();
        consume(lexer::TokenType::LEFT_BRACE, "Expected '{' after match value");
        std::vector<std::pair<ast::ExprPtr, ast::StmtPtr>> cases;
        ast::StmtPtr defaultCase = nullptr;
        while (!check(lexer::TokenType::RIGHT_BRACE) && !isAtEnd())
        {
            if (match(lexer::TokenType::CASE))
            {
                auto pattern = expression();
                consume(lexer::TokenType::COLON, "Expected ':' after case pattern");
                consume(lexer::TokenType::LEFT_BRACE, "Expected '{' after case pattern");
                auto body = blockStmt();
                cases.emplace_back(pattern, body);
            }
            else if (match(lexer::TokenType::DEFAULT))
            {
                consume(lexer::TokenType::COLON, "Expected ':' after default");
                consume(lexer::TokenType::LEFT_BRACE, "Expected '{' after default");
                defaultCase = blockStmt();
            }
            else
            {
                error(peek(), "Expected 'case' or 'default'");
                advance();
            }
        }
        consume(lexer::TokenType::RIGHT_BRACE, "Expected '}' after match");
        return std::make_shared<ast::MatchStmt>(value->token, value, cases, defaultCase);
    }

    ast::ExprPtr Parser::expression()
    {
        return assignment();
    }

    ast::ExprPtr Parser::elvis()
    {
        // a ?: b / a ?? b : yield a when non-null, else b. Binds looser than
        // logical-or and tighter than assignment.
        auto expr = orExpr();
        while (match(lexer::TokenType::ELVIS) || match(lexer::TokenType::NULL_COALESCE))
        {
            auto op = previous();
            auto right = orExpr();
            expr = std::make_shared<ast::BinaryExpr>(op, expr, op, right);
        }
        return expr;
    }

    ast::ExprPtr Parser::assignment()
    {
        ast::ExprPtr expr = elvis();

        if (match(lexer::TokenType::EQUAL))
        {
            lexer::Token equals = previous();
            ast::ExprPtr value = assignment();

            if (auto var = std::dynamic_pointer_cast<ast::VariableExpr>(expr))
            {
                return std::make_shared<ast::AssignExpr>(equals, var->name, value);
            }
            else if (auto get = std::dynamic_pointer_cast<ast::GetExpr>(expr))
            {
                return std::make_shared<ast::SetExpr>(
                    equals, get->object, get->name, value);
            }
            else if (std::dynamic_pointer_cast<ast::IndexExpr>(expr))
            {
                // arr[i] = value -> AssignExpr with the IndexExpr as target.
                return std::make_shared<ast::AssignExpr>(equals, expr, value);
            }

            errorHandler.reportError(error::ErrorCode::S005_INVALID_ASSIGNMENT_TARGET,
                                     "Invalid assignment target", equals,
                                     error::ErrorSeverity::ERROR);
        }

        return expr;
    }

    ast::ExprPtr Parser::orExpr()
    {
        auto expr = andExpr();
        while (match(lexer::TokenType::OR))
        {
            auto op = previous();
            auto right = andExpr();
            expr = std::make_shared<ast::BinaryExpr>(op, expr, op, right);
        }
        return expr;
    }

    ast::ExprPtr Parser::andExpr()
    {
        auto expr = equality();
        while (match(lexer::TokenType::AND))
        {
            auto op = previous();
            auto right = equality();
            expr = std::make_shared<ast::BinaryExpr>(op, expr, op, right);
        }
        return expr;
    }

    ast::ExprPtr Parser::equality()
    {
        auto expr = comparison();
        while (match(lexer::TokenType::EQUAL_EQUAL) || match(lexer::TokenType::BANG_EQUAL))
        {
            auto op = previous();
            auto right = comparison();
            expr = std::make_shared<ast::BinaryExpr>(op, expr, op, right);
        }
        return expr;
    }

    ast::ExprPtr Parser::comparison()
    {
        auto expr = term();
        while (match(lexer::TokenType::LESS) || match(lexer::TokenType::LESS_EQUAL) ||
               match(lexer::TokenType::GREATER) || match(lexer::TokenType::GREATER_EQUAL))
        {
            auto op = previous();
            auto right = term();
            expr = std::make_shared<ast::BinaryExpr>(op, expr, op, right);
        }
        return expr;
    }

    ast::ExprPtr Parser::term()
    {
        auto expr = factor();
        while (match(lexer::TokenType::PLUS) || match(lexer::TokenType::MINUS))
        {
            auto op = previous();
            auto right = factor();
            expr = std::make_shared<ast::BinaryExpr>(op, expr, op, right);
        }
        return expr;
    }

    ast::ExprPtr Parser::factor()
    {
        auto expr = unary();
        while (match(lexer::TokenType::STAR) || match(lexer::TokenType::SLASH) ||
               match(lexer::TokenType::PERCENT))
        {
            auto op = previous();
            auto right = unary();
            expr = std::make_shared<ast::BinaryExpr>(op, expr, op, right);
        }
        return expr;
    }

    ast::ExprPtr Parser::unary()
    {
        if (match(lexer::TokenType::BANG) || match(lexer::TokenType::MINUS))
        {
            lexer::Token op = previous();
            ast::ExprPtr right = unary();
            return std::make_shared<ast::UnaryExpr>(op, op, right);
        }
        if (match(lexer::TokenType::AWAIT))
        {
            auto expr = unary();
            return std::make_shared<ast::AwaitExpr>(previous(), expr);
        }
        if (match(lexer::TokenType::NEW))
        {
            return newExpr();
        }
        if (match(lexer::TokenType::DELETE))
        {
            return deleteExpr();
        }
        // A `<-` (or `-<`) in prefix position is a channel receive: `<-ch`.
        if (match(lexer::TokenType::CHANNEL_RECEIVE) || match(lexer::TokenType::CHANNEL_SEND))
        {
            lexer::Token op = previous();
            auto ch = unary();
            return std::make_shared<ast::ChannelReceiveExpr>(op, ch);
        }
        return call();
    }

    ast::ExprPtr Parser::call()
    {
        auto expr = primary();
        while (true)
        {
            if (match(lexer::TokenType::LEFT_PAREN))
            {
                std::vector<ast::ExprPtr> arguments;
                if (!check(lexer::TokenType::RIGHT_PAREN))
                {
                    do
                    {
                        arguments.push_back(expression());
                    } while (match(lexer::TokenType::COMMA));
                }
                auto paren = consume(lexer::TokenType::RIGHT_PAREN, "Expected ')' after arguments");
                expr = std::make_shared<ast::CallExpr>(paren, expr, arguments);
            }
            else if (match(lexer::TokenType::DOT))
            {
                auto name = consume(lexer::TokenType::IDENTIFIER, "Expected property name after '.'");
                expr = std::make_shared<ast::GetExpr>(name, expr, name.value);
            }
            else if (match(lexer::TokenType::SAFE_ACCESS))
            {
                // a?.b : safe navigation (null base yields null).
                auto name = consume(lexer::TokenType::IDENTIFIER, "Expected property name after '?.'");
                expr = std::make_shared<ast::GetExpr>(name, expr, name.value, /*isSafe=*/true);
            }
            else if (match(lexer::TokenType::BANG_BANG))
            {
                // a!! : force-unwrap (trap if null).
                lexer::Token op = previous();
                expr = std::make_shared<ast::UnaryExpr>(op, op, expr);
            }
            else if (match(lexer::TokenType::LEFT_BRACKET))
            {
                auto index = expression();
                auto bracket = consume(lexer::TokenType::RIGHT_BRACKET, "Expected ']' after index");
                expr = std::make_shared<ast::IndexExpr>(bracket, expr, index);
            }
            else if (match(lexer::TokenType::CHANNEL_SEND))
            {
                auto value = expression();
                expr = std::make_shared<ast::ChannelSendExpr>(previous(), expr, value);
            }
            else
            {
                break;
            }
        }
        return expr;
    }

    ast::ExprPtr Parser::primary()
    {
        if (match(lexer::TokenType::TRUE))
        {
            return std::make_shared<ast::LiteralExpr>(
                previous(), "true", ast::LiteralExpr::LiteralType::BOOLEAN);
        }
        if (match(lexer::TokenType::FALSE))
        {
            return std::make_shared<ast::LiteralExpr>(
                previous(), "false", ast::LiteralExpr::LiteralType::BOOLEAN);
        }
        if (match(lexer::TokenType::NIL))
        {
            return std::make_shared<ast::LiteralExpr>(
                previous(), "None", ast::LiteralExpr::LiteralType::NIL);
        }
        if (match(lexer::TokenType::INT) || match(lexer::TokenType::FLOAT64) ||
            match(lexer::TokenType::FLOAT32))
        {
            auto type = previous().type == lexer::TokenType::INT ? ast::LiteralExpr::LiteralType::INTEGER : ast::LiteralExpr::LiteralType::FLOAT;
            return std::make_shared<ast::LiteralExpr>(previous(), previous().value, type);
        }
        if (match(lexer::TokenType::STRING))
        {
            return std::make_shared<ast::LiteralExpr>(
                previous(), previous().value, ast::LiteralExpr::LiteralType::STRING);
        }
        if (match(lexer::TokenType::IDENTIFIER))
        {
            return std::make_shared<ast::VariableExpr>(previous(), previous().value);
        }
        if (match(lexer::TokenType::CHANNEL))
        {
            // channel<T>() or channel() -> a new channel handle.
            lexer::Token tok = previous();
            if (match(lexer::TokenType::LESS))
            {
                parseType();
                consume(lexer::TokenType::GREATER, "Expected '>' after channel element type");
            }
            consume(lexer::TokenType::LEFT_PAREN, "Expected '(' after channel");
            consume(lexer::TokenType::RIGHT_PAREN, "Expected ')' after channel(");
            return std::make_shared<ast::CallExpr>(
                tok, std::make_shared<ast::VariableExpr>(tok, "__chan_new"),
                std::vector<ast::ExprPtr>{});
        }
        if (match(lexer::TokenType::LEFT_PAREN))
        {
            auto expr = expression();
            consume(lexer::TokenType::RIGHT_PAREN, "Expected ')' after expression");
            return std::make_shared<ast::GroupingExpr>(expr->token, expr);
        }
        if (match(lexer::TokenType::LEFT_BRACKET))
        {
            std::vector<ast::ExprPtr> elements;
            if (!check(lexer::TokenType::RIGHT_BRACKET))
            {
                do
                {
                    elements.push_back(expression());
                } while (match(lexer::TokenType::COMMA));
            }
            auto token = consume(lexer::TokenType::RIGHT_BRACKET, "Expected ']' after list");
            return std::make_shared<ast::ListExpr>(token, elements);
        }
        if (match(lexer::TokenType::LEFT_BRACE))
        {
            std::vector<std::pair<ast::ExprPtr, ast::ExprPtr>> entries;
            if (!check(lexer::TokenType::RIGHT_BRACE))
            {
                do
                {
                    auto key = expression();
                    consume(lexer::TokenType::COLON, "Expected ':' after dictionary key");
                    auto value = expression();
                    entries.emplace_back(key, value);
                } while (match(lexer::TokenType::COMMA));
            }
            auto token = consume(lexer::TokenType::RIGHT_BRACE, "Expected '}' after dictionary");
            return std::make_shared<ast::DictionaryExpr>(token, entries);
        }
        if (match(lexer::TokenType::LAMBDA))
        {
            consume(lexer::TokenType::LEFT_PAREN, "Expected '(' after lambda");
            auto parameters = parseParameters();
            consume(lexer::TokenType::RIGHT_PAREN, "Expected ')' after lambda parameters");
            ast::TypePtr returnType = nullptr;
            if (match(lexer::TokenType::ARROW))
            {
                returnType = parseType();
            }
            else
            {
                returnType = std::make_shared<ast::SimpleType>(
                    lexer::Token(lexer::TokenType::NIL, "None", "", 0, 0));
            }
            auto body = expression();
            return std::make_shared<ast::LambdaExpr>(previous(), parameters, returnType, body);
        }
        error(peek(), "Expected expression");
        throw std::runtime_error("Parse error");
    }

    ast::TypePtr Parser::parseType()
    {
        // Accept the `channel` keyword as a type name (channel<T>).
        lexer::Token token = (check(lexer::TokenType::CHANNEL))
                                 ? advance()
                                 : consume(lexer::TokenType::IDENTIFIER, "Expected type name");
        if (match(lexer::TokenType::LESS))
        {
            std::vector<ast::TypePtr> typeArgs;
            do
            {
                typeArgs.push_back(parseType());
            } while (match(lexer::TokenType::COMMA));
            consume(lexer::TokenType::GREATER, "Expected '>' after type arguments");
            return std::make_shared<ast::GenericType>(token, token.value, typeArgs);
        }
        if (match(lexer::TokenType::LEFT_PAREN))
        {
            std::vector<ast::TypePtr> paramTypes;
            if (!check(lexer::TokenType::RIGHT_PAREN))
            {
                do
                {
                    paramTypes.push_back(parseType());
                } while (match(lexer::TokenType::COMMA));
            }
            consume(lexer::TokenType::RIGHT_PAREN, "Expected ')' after function type parameters");
            consume(lexer::TokenType::ARROW, "Expected '->' in function type");
            auto returnType = parseType();
            return std::make_shared<ast::FunctionType>(token, paramTypes, returnType);
        }
        if (match(lexer::TokenType::OR))
        {
            std::vector<ast::TypePtr> types = {std::make_shared<ast::SimpleType>(token)};
            do
            {
                types.push_back(parseType());
            } while (match(lexer::TokenType::OR));
            return std::make_shared<ast::UnionType>(token, types);
        }
        return std::make_shared<ast::SimpleType>(token);
    }

    std::vector<ast::Parameter> Parser::parseParameters()
    {
        std::vector<ast::Parameter> parameters;
        if (!check(lexer::TokenType::RIGHT_PAREN))
        {
            do
            {
                // Method receiver: a leading `self` with no required type.
                if (check(lexer::TokenType::SELF) ||
                    (check(lexer::TokenType::IDENTIFIER) && peek().value == "self"))
                {
                    auto selfTok = advance();
                    ast::TypePtr selfType;
                    if (match(lexer::TokenType::COLON))
                        selfType = parseType();
                    else
                        selfType = std::make_shared<ast::SimpleType>(
                            lexer::Token(lexer::TokenType::IDENTIFIER, "Self", "",
                                         selfTok.line, selfTok.column));
                    parameters.emplace_back("self", selfType);
                    continue;
                }
                auto name = consume(lexer::TokenType::IDENTIFIER, "Expected parameter name");
                consume(lexer::TokenType::COLON, "Expected ':' after parameter name");
                auto type = parseType();
                if (!type)
                {
                    error(peek(), "Failed to parse parameter type");
                    // Create a dummy type to avoid crash
                    type = std::make_shared<ast::SimpleType>(
                        lexer::Token(lexer::TokenType::IDENTIFIER, "int", "", 0, 0));
                }
                parameters.emplace_back(name.value, type);
            } while (match(lexer::TokenType::COMMA));
        }
        return parameters;
    }

    void Parser::synchronize()
    {
        advance();
        while (!isAtEnd())
        {
            if (previous().type == lexer::TokenType::SEMI_COLON)
                return;
            switch (peek().type)
            {
            case lexer::TokenType::CLASS:
            case lexer::TokenType::DEF:
            case lexer::TokenType::ASYNC:
            case lexer::TokenType::LET:
            case lexer::TokenType::CONST:
            case lexer::TokenType::FOR:
            case lexer::TokenType::IF:
            case lexer::TokenType::WHILE:
            case lexer::TokenType::RETURN:
            case lexer::TokenType::IMPORT:
            case lexer::TokenType::MATCH:
            case lexer::TokenType::GO:
            case lexer::TokenType::SELECT:
                return;
            default:
                advance();
            }
        }
    }

    bool Parser::match(lexer::TokenType type)
    {
        if (check(type))
        {
            advance();
            return true;
        }
        return false;
    }

    bool Parser::check(lexer::TokenType type) const
    {
        if (isAtEnd())
            return false;
        return peek().type == type;
    }

    lexer::Token Parser::advance()
    {
        if (!isAtEnd())
            ++current;
        return previous();
    }

    lexer::Token Parser::peek() const
    {
        return tokens[current];
    }

    lexer::Token Parser::previous() const
    {
        return tokens[current - 1];
    }

    bool Parser::isAtEnd() const
    {
        return peek().type == lexer::TokenType::EOF_TOKEN;
    }

    lexer::Token Parser::consume(lexer::TokenType type, const std::string &message)
    {
        if (check(type))
            return advance();

        error(peek(), message);
        return peek();
    }

    void Parser::error(const lexer::Token &token, const std::string &message)
    {
        errorHandler.reportError(error::ErrorCode::S001_UNEXPECTED_TOKEN,
                                 message, token,
                                 error::ErrorSeverity::ERROR);
        recordError(message, token, {}, false);
    }
    
    void Parser::recordError(const std::string& message, const lexer::Token& token,
                            const std::vector<lexer::TokenType>& expected, bool isFatal) {
        ErrorContext ctx;
        ctx.message = message;
        ctx.token = token;
        ctx.expectedTokens = expected;
        ctx.isFatal = isFatal;
        errors_.push_back(ctx);
    }
    
    bool Parser::synchronizeToToken(lexer::TokenType type) {
        while (!isAtEnd()) {
            if (check(type)) {
                return true;
            }
            advance();
        }
        return false;
    }
    
    bool Parser::synchronizeToAny(const std::vector<lexer::TokenType>& types) {
        while (!isAtEnd()) {
            for (auto type : types) {
                if (check(type)) {
                    return true;
                }
            }
            advance();
        }
        return false;
    }
    
    void Parser::skipUntilSynchronizationPoint() {
        static const std::vector<lexer::TokenType> syncPoints = {
            lexer::TokenType::CLASS,
            lexer::TokenType::DEF,
            lexer::TokenType::LET,
            lexer::TokenType::CONST,
            lexer::TokenType::IF,
            lexer::TokenType::WHILE,
            lexer::TokenType::FOR,
            lexer::TokenType::RETURN,
            lexer::TokenType::IMPORT,
            lexer::TokenType::RIGHT_BRACE,
            lexer::TokenType::SEMI_COLON
        };
        synchronizeToAny(syncPoints);
    }
    
    int Parser::getOperatorPrecedence(lexer::TokenType type) const {
        switch (type) {
            // Assignment (lowest)
            case lexer::TokenType::EQUAL: return 1;
            
            // Logical OR
            case lexer::TokenType::OR: return 2;
            
            // Logical AND
            case lexer::TokenType::AND: return 3;
            
            // Equality
            case lexer::TokenType::EQUAL_EQUAL:
            case lexer::TokenType::BANG_EQUAL: return 4;
            
            // Relational
            case lexer::TokenType::LESS:
            case lexer::TokenType::LESS_EQUAL:
            case lexer::TokenType::GREATER:
            case lexer::TokenType::GREATER_EQUAL: return 5;
            
            // Bitwise OR
            case lexer::TokenType::BITWISE_OR: return 6;
            
            // Bitwise XOR
            case lexer::TokenType::BITWISE_XOR: return 7;
            
            // Bitwise AND
            case lexer::TokenType::BITWISE_AND: return 8;
            
            // Shift
            case lexer::TokenType::LEFT_SHIFT:
            case lexer::TokenType::RIGHT_SHIFT: return 9;
            
            // Additive
            case lexer::TokenType::PLUS:
            case lexer::TokenType::MINUS: return 10;
            
            // Multiplicative
            case lexer::TokenType::STAR:
            case lexer::TokenType::SLASH:
            case lexer::TokenType::PERCENT: return 11;
            
            // Exponentiation (highest binary)
            case lexer::TokenType::POWER: return 12;
            
            default: return 0;
        }
    }
    
    bool Parser::isRightAssociative(lexer::TokenType type) const {
        return type == lexer::TokenType::EQUAL || 
               type == lexer::TokenType::POWER;
    }
    
    ast::ExprPtr Parser::parseBinaryExpression(int minPrecedence) {
        auto left = unary();
        
        while (!isAtEnd()) {
            lexer::TokenType opType = peek().type;
            int precedence = getOperatorPrecedence(opType);
            
            if (precedence < minPrecedence) {
                break;
            }
            
            auto op = advance();
            
            int nextMinPrecedence = precedence;
            if (!isRightAssociative(opType)) {
                nextMinPrecedence++;
            }
            
            auto right = parseBinaryExpression(nextMinPrecedence);
            if (!right) {
                recordError("Expected expression after operator", op, {}, false);
                return nullptr;
            }
            
            left = std::make_shared<ast::BinaryExpr>(op, left, op, right);
        }
        
        return left;
    }
    
    bool Parser::validateExpression(ast::ExprPtr expr) {
        if (!expr) return false;
        
        // Add expression validation logic here
        // Check for:
        // - Null dereference
        // - Type consistency
        // - Valid operations
        
        return true;
    }
    
    bool Parser::validateStatement(ast::StmtPtr stmt) {
        if (!stmt) return false;
        
        // Add statement validation logic here
        // Check for:
        // - Unreachable code
        // - Missing return statements
        // - Variable shadowing
        
        return true;
    }
    
    bool Parser::validateType(ast::TypePtr type) {
        if (!type) return false;
        
        // Add type validation logic here
        // Check for:
        // - Unknown types
        // - Invalid generic instantiations
        // - Circular dependencies
        
        return true;
    }

    ast::ExprPtr Parser::newExpr()
    {
        lexer::Token keyword = previous();
        ast::ExprPtr expr = primary();

        // Handle array allocation if followed by square brackets
        if (match(lexer::TokenType::LEFT_BRACKET))
        {
            ast::ExprPtr size = expression();
            consume(lexer::TokenType::RIGHT_BRACKET, "Expect ']' after array size.");
            return std::make_shared<ast::NewExpr>(keyword, std::move(expr), std::move(size));
        }

        return std::make_shared<ast::NewExpr>(keyword, std::move(expr), nullptr);
    }

    ast::ExprPtr Parser::deleteExpr()
    {
        lexer::Token keyword = previous();
        ast::ExprPtr expr = primary();
        return std::make_shared<ast::DeleteExpr>(keyword, std::move(expr));
    }

    ast::StmtPtr Parser::goStmt()
    {
        auto keyword = previous();
        // Accept both `go f(args);` and `go (f(args));`.
        auto expr = expression();
        consume(lexer::TokenType::SEMI_COLON, "Expected ';' after goroutine statement");
        return std::make_shared<ast::GoStmt>(keyword, expr);
    }

    ast::StmtPtr Parser::selectStmt()
    {
        auto keyword = previous();
        consume(lexer::TokenType::LEFT_BRACE, "Expected '{' after 'select'");
        
        std::vector<ast::SelectStmt::Case> cases;
        
        while (!check(lexer::TokenType::RIGHT_BRACE) && !isAtEnd())
        {
            if (match(lexer::TokenType::CASE))
            {
                // Parse channel send or receive case
                ast::ExprPtr channel = nullptr;
                ast::ExprPtr value = nullptr;
                bool isSend = false;
                
                // Check if this is a send case (channel <- value)
                auto firstExpr = expression();
                
                if (match(lexer::TokenType::CHANNEL_SEND))
                {
                    // This is a send case: channel <- value
                    channel = firstExpr;
                    value = expression();
                    isSend = true;
                }
                else
                {
                    // This is a receive case: <-channel or value := <-channel
                    if (firstExpr->token.type == lexer::TokenType::CHANNEL_RECEIVE)
                    {
                        // Simple receive: <-channel
                        channel = expression();
                    }
                    else
                    {
                        // Assignment receive: value := <-channel
                        value = firstExpr;
                        consume(lexer::TokenType::CHANNEL_RECEIVE, "Expected '<-' for channel receive");
                        channel = expression();
                    }
                }
                
                consume(lexer::TokenType::COLON, "Expected ':' after case");
                auto body = blockStmt();
                
                cases.emplace_back(channel, body, false);
            }
            else if (match(lexer::TokenType::DEFAULT))
            {
                consume(lexer::TokenType::COLON, "Expected ':' after default");
                auto body = blockStmt();
                
                // Create a default case with null channel
                cases.emplace_back(nullptr, body, true);
            }
            else
            {
                error(peek(), "Expected 'case' or 'default' in select statement");
                advance();
            }
        }
        
        consume(lexer::TokenType::RIGHT_BRACE, "Expected '}' after select statement");
        return std::make_shared<ast::SelectStmt>(keyword, cases);
    }

    ast::ExprPtr Parser::channelSendExpr()
    {
        auto channel = expression();
        consume(lexer::TokenType::CHANNEL_SEND, "Expected '<-' for channel send");
        auto value = expression();
        
        return std::make_shared<ast::ChannelSendExpr>(previous(), channel, value);
    }

    ast::ExprPtr Parser::channelReceiveExpr()
    {
        auto keyword = previous();
        auto channel = expression();
        
        return std::make_shared<ast::ChannelReceiveExpr>(keyword, channel);
    }

} // namespace parser
