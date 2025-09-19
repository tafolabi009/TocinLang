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
    {
        // Remove quotes from the string literal
        std::string str = expr->value.lexeme;
        if (str.size() >= 2)
        {
            str = str.substr(1, str.size() - 2);
        }

        // Handle escape sequences
        std::string processedStr;
        for (size_t i = 0; i < str.size(); ++i)
        {
            if (str[i] == '\\' && i + 1 < str.size())
            {
                switch (str[i + 1])
                {
                case 'n':
                    processedStr += '\n';
                    break;
                case 't':
                    processedStr += '\t';
                    break;
                case 'r':
                    processedStr += '\r';
                    break;
                case '\\':
                    processedStr += '\\';
                    break;
                case '\"':
                    processedStr += '\"';
                    break;
                case '\'':
                    processedStr += '\'';
                    break;
                default:
                    processedStr += str[i];
                    processedStr += str[i + 1];
                }
                ++i; // Skip the next character
            }
            else
            {
                processedStr += str[i];
            }
        }

        // Create a global string with null terminator
        lastValue = builder.CreateGlobalStringPtr(processedStr, "str");
    }
    else if (expr->value.getType() == lexer::TokenType::TRUE_KW)
    {
        lastValue = llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 1);
    }
    else if (expr->value.getType() == lexer::TokenType::FALSE_KW)
    {
        lastValue = llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 0);
    }
    else if (expr->value.getType() == lexer::TokenType::NIL_KW)
    {
        // Nil is represented as a null pointer
        lastValue = llvm::ConstantPointerNull::get(
            llvm::PointerType::get(context, 0));
    }
    else
    {
        errorHandler.reportError(error::ErrorCode::C003_TYPECHECK_ERROR,
                                 "Unsupported literal type: " + expr->value.lexeme,
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        lastValue = nullptr;
    }
}

void IRGenerator::visitVariableStmt(ast::VariableStmt *stmt)
{
    // Get the variable type
    llvm::Type *varType = nullptr;

    if (stmt->type)
    {
        // If type is explicitly specified
        varType = getLLVMType(stmt->type);
    }
    else if (stmt->initializer)
    {
        // If type is inferred from initializer
        stmt->initializer->accept(*this);
        if (!lastValue)
            return;

        varType = lastValue->getType();
    }
    else
    {
        // No type and no initializer - error
        errorHandler.reportError(error::ErrorCode::T003_TYPE_INFERENCE_FAILED,
                                 "Cannot infer type for variable '" + stmt->name.lexeme + "' without initializer",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        return;
    }

    if (!varType)
    {
        errorHandler.reportError(error::ErrorCode::T004_UNDEFINED_TYPE,
                                 "Unknown type for variable '" + stmt->name.lexeme + "'",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        return;
    }

    // Create an alloca instruction in the entry block of the current function
    llvm::AllocaInst *alloca = createEntryBlockAlloca(currentFunction, stmt->name.lexeme, varType);

    // Store the variable in the symbol table
    namedValues[stmt->name.lexeme] = alloca;

    // If there's an initializer, store its value
    if (stmt->initializer)
    {
        if (!lastValue)
        {
            // If we don't have a value yet, compute the initializer
            stmt->initializer->accept(*this);
            if (!lastValue)
                return;
        }

        // Validate that initializer type matches variable type
        if (lastValue->getType() != varType)
        {
            // Simple cast for numeric values
            if (lastValue->getType()->isIntegerTy() && varType->isIntegerTy())
            {
                lastValue = builder.CreateIntCast(lastValue, varType, true, "cast");
            }
            else if ((lastValue->getType()->isFloatTy() || lastValue->getType()->isDoubleTy()) &&
                     (varType->isFloatTy() || varType->isDoubleTy()))
            {
                lastValue = builder.CreateFPCast(lastValue, varType, "cast");
            }
            else
            {
                errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                         "Initializer type does not match variable type",
                                         "", 0, 0, error::ErrorSeverity::ERROR);
                return;
            }
        }

        // Store the initial value
        builder.CreateStore(lastValue, alloca);
    }
}

void IRGenerator::visitVariableExpr(ast::VariableExpr *expr)
{
    // Look up the variable in the symbol table
    auto it = namedValues.find(expr->name.lexeme);
    if (it == namedValues.end())
    {
        errorHandler.reportError(error::ErrorCode::T005_UNDEFINED_VARIABLE,
                                 "Undefined variable '" + expr->name.lexeme + "'",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        lastValue = nullptr;
        return;
    }

    // Load the value
    lastValue = builder.CreateLoad(it->second->getAllocatedType(), it->second, expr->name.lexeme);
}

void IRGenerator::visitAssignExpr(ast::AssignExpr *expr)
{
    // Look up the variable in the symbol table
    auto it = namedValues.find(expr->name.lexeme);
    if (it == namedValues.end())
    {
        errorHandler.reportError(error::ErrorCode::T005_UNDEFINED_VARIABLE,
                                 "Undefined variable for assignment '" + expr->name.lexeme + "'",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        lastValue = nullptr;
        return;
    }

    // Evaluate the value
    expr->value->accept(*this);
    if (!lastValue)
        return;

    // Validate type compatibility
    llvm::Type *varType = it->second->getAllocatedType();
    if (lastValue->getType() != varType)
    {
        // Simple cast for numeric values
        if (lastValue->getType()->isIntegerTy() && varType->isIntegerTy())
        {
            lastValue = builder.CreateIntCast(lastValue, varType, true, "cast");
        }
        else if ((lastValue->getType()->isFloatTy() || lastValue->getType()->isDoubleTy()) &&
                 (varType->isFloatTy() || varType->isDoubleTy()))
        {
            lastValue = builder.CreateFPCast(lastValue, varType, "cast");
        }
        else
        {
            errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                     "Assignment value type does not match variable type",
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            lastValue = nullptr;
            return;
        }
    }

    // Store the value
    builder.CreateStore(lastValue, it->second);
}

void IRGenerator::visitFunctionStmt(ast::FunctionStmt *stmt)
{
    // Get return type
    llvm::Type *returnType = getLLVMType(stmt->returnType);
    if (!returnType)
        return;
    
    // Get parameter types
    std::vector<llvm::Type *> paramTypes;
    for (const auto &param : stmt->params)
    {
        llvm::Type *paramType = getLLVMType(param.type);
        if (!paramType)
            return;
        
        paramTypes.push_back(paramType);
    }
    
    // Create function type
    llvm::FunctionType *functionType = llvm::FunctionType::get(returnType, paramTypes, false);
    
    // Create function
    llvm::Function *function = llvm::Function::Create(
        functionType, llvm::Function::ExternalLinkage, stmt->name.lexeme, module.get());
    
    // Set parameter names
    unsigned idx = 0;
    for (auto &param : function->args())
    {
        param.setName(stmt->params[idx++].name.lexeme);
