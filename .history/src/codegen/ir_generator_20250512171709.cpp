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
    }

    // Create basic block
    llvm::BasicBlock *block = llvm::BasicBlock::Create(context, "entry", function);

    // Save current insert point
    llvm::BasicBlock *savedBlock = builder.GetInsertBlock();
    llvm::Function *savedFunction = currentFunction;

    // Set new insert point
    builder.SetInsertPoint(block);
    currentFunction = function;

    // Save previous variables
    std::map<std::string, llvm::AllocaInst *> savedNamedValues(namedValues);
    namedValues.clear();

    // Create allocas for parameters
    idx = 0;
    for (auto &param : function->args())
    {
        llvm::AllocaInst *alloca = createEntryBlockAlloca(
            function, param.getName().str(), param.getType());

        // Store the parameter value
        builder.CreateStore(&param, alloca);

        // Add to symbol table
        namedValues[param.getName().str()] = alloca;

        idx++;
    }

    // Codegen function body
    stmt->body->accept(*this);

    // Add implicit return if needed
    if (!builder.GetInsertBlock()->getTerminator())
    {
        if (returnType->isVoidTy())
        {
            builder.CreateRetVoid();
        }
        else
        {
            // Insert a reasonable default return value
            if (returnType->isIntegerTy())
            {
                builder.CreateRet(llvm::ConstantInt::get(returnType, 0));
            }
            else if (returnType->isFloatTy() || returnType->isDoubleTy())
            {
                builder.CreateRet(llvm::ConstantFP::get(returnType, 0.0));
            }
            else if (returnType->isPointerTy())
            {
                builder.CreateRet(llvm::ConstantPointerNull::get(
                    llvm::cast<llvm::PointerType>(returnType)));
            }
            else
            {
                errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                         "Cannot generate default return value for type",
                                         "", 0, 0, error::ErrorSeverity::ERROR);
            }
        }
    }

    // Verify the function
    if (llvm::verifyFunction(*function, &llvm::errs()))
    {
        errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                 "Function verification failed",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        function->eraseFromParent();
        return;
    }

    // Restore previous state
    namedValues = savedNamedValues;
    currentFunction = savedFunction;

    if (savedBlock)
        builder.SetInsertPoint(savedBlock);
}

void IRGenerator::visitReturnStmt(ast::ReturnStmt *stmt)
{
    // Get return type of the current function
    llvm::Type *returnType = currentFunction->getReturnType();

    if (stmt->value)
    {
        // Evaluate return value
        stmt->value->accept(*this);
        if (!lastValue)
            return;

        // Check return type compatibility
        if (lastValue->getType() != returnType)
        {
            // Try to cast if possible
            if (lastValue->getType()->isIntegerTy() && returnType->isIntegerTy())
            {
                lastValue = builder.CreateIntCast(lastValue, returnType, true, "castret");
            }
            else if ((lastValue->getType()->isFloatTy() || lastValue->getType()->isDoubleTy()) &&
                     (returnType->isFloatTy() || returnType->isDoubleTy()))
            {
                lastValue = builder.CreateFPCast(lastValue, returnType, "castret");
            }
            else
            {
                errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                         "Return value type does not match function return type",
                                         "", 0, 0, error::ErrorSeverity::ERROR);
                return;
            }
        }

        // Create return instruction
        builder.CreateRet(lastValue);
    }
    else
    {
        // Void return
        if (!returnType->isVoidTy())
        {
            errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                     "Missing return value in non-void function",
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            return;
        }

        builder.CreateRetVoid();
    }
}

void IRGenerator::visitCallExpr(ast::CallExpr *expr)
{
    // Evaluate callee
    expr->callee->accept(*this);
    llvm::Value *callee = lastValue;

    if (!callee)
        return;

    // Handle special case - direct function call by name
    if (auto *varExpr = std::dynamic_pointer_cast<ast::VariableExpr>(expr->callee))
    {
        std::string funcName = varExpr->name.lexeme;

        // Check for standard library functions
        if (stdLibFunctions.find(funcName) != stdLibFunctions.end())
        {
            callee = stdLibFunctions[funcName];
        }
        else
        {
            // Try to find function in the module
            llvm::Function *func = module->getFunction(funcName);

            if (func)
            {
                callee = func;
            }
        }
    }

    // Ensure callee is a function
    if (!callee->getType()->isPointerTy() ||
        !llvm::cast<llvm::PointerType>(callee->getType())->getElementType()->isFunctionTy())
    {
        errorHandler.reportError(error::ErrorCode::T006_INVALID_OPERATOR_FOR_TYPE,
                                 "Called value is not a function",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        lastValue = nullptr;
        return;
    }

    // Get function type
    llvm::FunctionType *funcType = llvm::cast<llvm::FunctionType>(
        llvm::cast<llvm::PointerType>(callee->getType())->getElementType());

    // Check argument count
    if (funcType->getNumParams() != expr->arguments.size())
    {
        errorHandler.reportError(error::ErrorCode::T002_WRONG_ARGUMENT_COUNT,
                                 "Wrong number of arguments to function call",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        lastValue = nullptr;
        return;
    }

    // Evaluate arguments
    std::vector<llvm::Value *> args;
    for (size_t i = 0; i < expr->arguments.size(); ++i)
    {
        expr->arguments[i]->accept(*this);
        if (!lastValue)
            return;

        // Check argument type compatibility and cast if needed
        if (lastValue->getType() != funcType->getParamType(i))
        {
            if (lastValue->getType()->isIntegerTy() && funcType->getParamType(i)->isIntegerTy())
            {
                lastValue = builder.CreateIntCast(lastValue, funcType->getParamType(i),
                                                  true, "castarg");
            }
            else if ((lastValue->getType()->isFloatTy() || lastValue->getType()->isDoubleTy()) &&
                     (funcType->getParamType(i)->isFloatTy() || funcType->getParamType(i)->isDoubleTy()))
            {
                lastValue = builder.CreateFPCast(lastValue, funcType->getParamType(i), "castarg");
            }
            else
            {
                errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                         "Argument type does not match parameter type",
                                         "", 0, 0, error::ErrorSeverity::ERROR);
                lastValue = nullptr;
                return;
            }
        }

        args.push_back(lastValue);
    }

    // Create call instruction
    lastValue = builder.CreateCall(funcType, callee, args);
}
void IRGenerator::visitIfStmt(ast::IfStmt *stmt)
{
    // Generate condition
    stmt->condition->accept(*this);
    if (!lastValue)
        return;
    
    // Convert condition to boolean if needed
