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
    llvm::Value *condValue = lastValue;
    if (!condValue->getType()->isIntegerTy(1))
    {
        if (condValue->getType()->isIntegerTy())
        {
            // Compare with zero for integers
            condValue = builder.CreateICmpNE(
                condValue,
                llvm::ConstantInt::get(condValue->getType(), 0),
                "ifcond");
        }
        else if (condValue->getType()->isFloatTy() || condValue->getType()->isDoubleTy())
        {
            // Compare with zero for floating point
            condValue = builder.CreateFCmpONE(
                condValue,
                llvm::ConstantFP::get(condValue->getType(), 0.0),
                "ifcond");
        }
        else if (condValue->getType()->isPointerTy())
        {
            // Compare with null for pointers
            condValue = builder.CreateICmpNE(
                condValue,
                llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(condValue->getType())),
                "ifcond");
        }
        else
        {
            errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                     "Condition must be convertible to a boolean",
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            return;
        }
    }

    // Create basic blocks for then, else, and continue
    llvm::Function *function = builder.GetInsertBlock()->getParent();

    llvm::BasicBlock *thenBlock = llvm::BasicBlock::Create(context, "then", function);
    llvm::BasicBlock *elseBlock = stmt->elseBranch ? llvm::BasicBlock::Create(context, "else") : nullptr;
    llvm::BasicBlock *continueBlock = llvm::BasicBlock::Create(context, "ifcont");

    // Create conditional branch
    if (elseBlock)
        builder.CreateCondBr(condValue, thenBlock, elseBlock);
    else
        builder.CreateCondBr(condValue, thenBlock, continueBlock);

    // Generate then branch
    builder.SetInsertPoint(thenBlock);

    // Save environment before entering new scope
    createEnvironment();

    stmt->thenBranch->accept(*this);

    // Restore environment after exiting scope
    restoreEnvironment();

    // Create branch to continue block if needed
    if (!builder.GetInsertBlock()->getTerminator())
        builder.CreateBr(continueBlock);

    // Generate else branch if it exists
    if (elseBlock)
    {
        function->getBasicBlockList().push_back(elseBlock);
        builder.SetInsertPoint(elseBlock);

        // Save environment before entering new scope
        createEnvironment();

        if (stmt->elseBranch)
            stmt->elseBranch->accept(*this);

        // Restore environment after exiting scope
        restoreEnvironment();

        // Create branch to continue block if needed
        if (!builder.GetInsertBlock()->getTerminator())
            builder.CreateBr(continueBlock);
    }

    // Continue with the next block
    function->getBasicBlockList().push_back(continueBlock);
    builder.SetInsertPoint(continueBlock);
}

void IRGenerator::visitWhileStmt(ast::WhileStmt *stmt)
{
    // Create basic blocks
    llvm::Function *function = builder.GetInsertBlock()->getParent();

    llvm::BasicBlock *condBlock = llvm::BasicBlock::Create(context, "whilecond", function);
    llvm::BasicBlock *loopBlock = llvm::BasicBlock::Create(context, "whilebody");
    llvm::BasicBlock *afterBlock = llvm::BasicBlock::Create(context, "whilecont");

    // Branch to condition
    builder.CreateBr(condBlock);

    // Generate condition
    builder.SetInsertPoint(condBlock);

    stmt->condition->accept(*this);
    if (!lastValue)
        return;

    // Convert condition to boolean if needed
    llvm::Value *condValue = lastValue;
    if (!condValue->getType()->isIntegerTy(1))
    {
        if (condValue->getType()->isIntegerTy())
        {
            // Compare with zero for integers
            condValue = builder.CreateICmpNE(
                condValue,
                llvm::ConstantInt::get(condValue->getType(), 0),
                "whilecond");
        }
        else if (condValue->getType()->isFloatTy() || condValue->getType()->isDoubleTy())
        {
            // Compare with zero for floating point
            condValue = builder.CreateFCmpONE(
                condValue,
                llvm::ConstantFP::get(condValue->getType(), 0.0),
                "whilecond");
        }
        else if (condValue->getType()->isPointerTy())
        {
            // Compare with null for pointers
            condValue = builder.CreateICmpNE(
                condValue,
                llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(condValue->getType())),
                "whilecond");
        }
        else
        {
            errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                     "Condition must be convertible to a boolean",
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            return;
        }
    }

    // Create conditional branch
    builder.CreateCondBr(condValue, loopBlock, afterBlock);

    // Generate loop body
    function->getBasicBlockList().push_back(loopBlock);
    builder.SetInsertPoint(loopBlock);

    // Save environment before entering new scope
    createEnvironment();

    stmt->body->accept(*this);

    // Restore environment after exiting scope
    restoreEnvironment();

    // Create branch back to condition
    if (!builder.GetInsertBlock()->getTerminator())
        builder.CreateBr(condBlock);

    // Continue with the next block
    function->getBasicBlockList().push_back(afterBlock);
    builder.SetInsertPoint(afterBlock);
}

void IRGenerator::visitForStmt(ast::ForStmt *stmt)
{
    // Create basic blocks
    llvm::Function *function = builder.GetInsertBlock()->getParent();

    // Save environment before entering new scope
    createEnvironment();

    // Generate initializer
    if (stmt->initializer)
        stmt->initializer->accept(*this);

    llvm::BasicBlock *condBlock = llvm::BasicBlock::Create(context, "forcond", function);
    llvm::BasicBlock *loopBlock = llvm::BasicBlock::Create(context, "forbody");
    llvm::BasicBlock *updateBlock = llvm::BasicBlock::Create(context, "forupdate");
    llvm::BasicBlock *afterBlock = llvm::BasicBlock::Create(context, "forcont");

    // Branch to condition
    builder.CreateBr(condBlock);

    // Generate condition
    builder.SetInsertPoint(condBlock);

    llvm::Value *condValue = nullptr;

    if (stmt->condition)
    {
        stmt->condition->accept(*this);
        if (!lastValue)
            return;

        condValue = lastValue;

        // Convert condition to boolean if needed
        if (!condValue->getType()->isIntegerTy(1))
        {
            if (condValue->getType()->isIntegerTy())
            {
                // Compare with zero for integers
                condValue = builder.CreateICmpNE(
                    condValue,
                    llvm::ConstantInt::get(condValue->getType(), 0),
                    "forcond");
            }
            else if (condValue->getType()->isFloatTy() || condValue->getType()->isDoubleTy())
            {
                // Compare with zero for floating point
                condValue = builder.CreateFCmpONE(
                    condValue,
                    llvm::ConstantFP::get(condValue->getType(), 0.0),
                    "forcond");
            }
            else if (condValue->getType()->isPointerTy())
            {
                // Compare with null for pointers
                condValue = builder.CreateICmpNE(
                    condValue,
                    llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(condValue->getType())),
                    "forcond");
            }
            else
            {
                errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                         "Condition must be convertible to a boolean",
                                         "", 0, 0, error::ErrorSeverity::ERROR);
                return;
            }
        }
    }
    else
    {
        // No condition, always true
        condValue = llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 1);
    }

    // Create conditional branch
    builder.CreateCondBr(condValue, loopBlock, afterBlock);

    // Generate loop body
    function->getBasicBlockList().push_back(loopBlock);
    builder.SetInsertPoint(loopBlock);

    stmt->body->accept(*this);

    // Branch to update block
    if (!builder.GetInsertBlock()->getTerminator())
        builder.CreateBr(updateBlock);

    // Generate update
    function->getBasicBlockList().push_back(updateBlock);
    builder.SetInsertPoint(updateBlock);

    if (stmt->increment)
        stmt->increment->accept(*this);

    // Branch back to condition
    builder.CreateBr(condBlock);

    // Continue with the next block
    function->getBasicBlockList().push_back(afterBlock);
    builder.SetInsertPoint(afterBlock);

    // Restore environment after exiting scope
    restoreEnvironment();
}

void IRGenerator::visitUnaryExpr(ast::UnaryExpr *expr)
{
    // Evaluate the operand
    expr->right->accept(*this);
    llvm::Value *operand = lastValue;

    if (!operand)
        return;

    switch (expr->op.type)
    {
    case lexer::TokenType::MINUS:
        if (operand->getType()->isIntegerTy())
        {
            lastValue = builder.CreateNeg(operand, "negtmp");
        }
        else if (operand->getType()->isFloatTy() || operand->getType()->isDoubleTy())
        {
            lastValue = builder.CreateFNeg(operand, "fnegtmp");
        }
        else
        {
            errorHandler.reportError(error::ErrorCode::T006_INVALID_OPERATOR_FOR_TYPE,
                                     "Invalid operand to unary -",
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            lastValue = nullptr;
        }
        break;

    case lexer::TokenType::BANG:
        if (operand->getType()->isIntegerTy(1))
        {
            // Boolean negation
            lastValue = builder.CreateNot(operand, "nottmp");
        }
        else if (operand->getType()->isIntegerTy())
        {
            // Compare with zero for integers
            lastValue = builder.CreateICmpEQ(
                operand,
                llvm::ConstantInt::get(operand->getType(), 0),
                "nottmp");
        }
        else if (operand->getType()->isFloatTy() || operand->getType()->isDoubleTy())
        {
            // Compare with zero for floating point
            lastValue = builder.CreateFCmpOEQ(
                operand,
                llvm::ConstantFP::get(operand->getType(), 0.0),
                "nottmp");
        }
        else if (operand->getType()->isPointerTy())
        {
            // Compare with null for pointers
            lastValue = builder.CreateICmpEQ(
                operand,
                llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(operand->getType())),
                "nottmp");
        }
        else
        {
            errorHandler.reportError(error::ErrorCode::T006_INVALID_OPERATOR_FOR_TYPE,
                                     "Invalid operand to unary !",
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            lastValue = nullptr;
        }
        break;

    default:
        errorHandler.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                                 "Unhandled unary operator",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        lastValue = nullptr;
        break;
    }
}

void IRGenerator::visitLambdaExpr(ast::LambdaExpr *expr)
{
    // Get return type
    llvm::Type *returnType = getLLVMType(expr->returnType);
    if (!returnType)
        return;

    // Get parameter types
    std::vector<llvm::Type *> paramTypes;
    for (const auto &param : expr->params)
    {
        llvm::Type *paramType = getLLVMType(param.type);
        if (!paramType)
            return;

        paramTypes.push_back(paramType);
    }

    // Create function type
    llvm::FunctionType *functionType = llvm::FunctionType::get(returnType, paramTypes, false);

    // Create unique name for the lambda
    static int lambdaCounter = 0;
    std::string lambdaName = "lambda_" + std::to_string(lambdaCounter++);

    // Create function
    llvm::Function *function = llvm::Function::Create(
        functionType, llvm::Function::InternalLinkage, lambdaName, module.get());

    // Set parameter names
    unsigned idx = 0;
    for (auto &param : function->args())
    {
        param.setName(expr->params[idx++].name.lexeme);
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
    expr->body->accept(*this);

    // Add implicit return if needed
    if (!builder.GetInsertBlock()->getTerminator())
    {
        if (returnType->isVoidTy())
        {
            builder.CreateRetVoid();
        }
        else if (lastValue && lastValue->getType() == returnType)
        {
            // Use the last expression's value as the return value
            builder.CreateRet(lastValue);
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
                                         "Cannot generate default return value for lambda",
                                         "", 0, 0, error::ErrorSeverity::ERROR);
                function->eraseFromParent();
                lastValue = nullptr;
                return;
            }
        }
    }

    // Verify the function
    if (llvm::verifyFunction(*function, &llvm::errs()))
    {
        errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                 "Lambda verification failed",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        function->eraseFromParent();
        lastValue = nullptr;
        return;
    }

    // Restore previous state
    namedValues = savedNamedValues;
    currentFunction = savedFunction;

    if (savedBlock)
        builder.SetInsertPoint(savedBlock);

    // Return the function as a value
    lastValue = function;
}

void IRGenerator::visitListExpr(ast::ListExpr *expr)
{
    if (expr->elements.empty())
    {
        // Create empty list
        createEmptyList(expr->type);
        return;
    }

    // Evaluate the first element to determine type
    expr->elements[0]->accept(*this);
    llvm::Value *firstElement = lastValue;
    if (!firstElement)
        return;

    // Get element type
    llvm::Type *elementType = firstElement->getType();

    // Create list struct type: { int64_t length, elementType* data }
    std::vector<llvm::Type *> listFields = {
        llvm::Type::getInt64Ty(context),          // length
        llvm::PointerType::getUnqual(elementType) // data
    };
    llvm::StructType *listType = llvm::StructType::get(context, listFields);

    // Allocate list struct
    llvm::AllocaInst *listAlloc = builder.CreateAlloca(listType, nullptr, "list");

    // Set length
    llvm::Value *lengthPtr = builder.CreateStructGEP(listType, listAlloc, 0, "list.length");
    builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), expr->elements.size()), lengthPtr);

    // Allocate array for elements
    llvm::Value *arraySize = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), expr->elements.size());
    llvm::Value *dataPtr = builder.CreateArrayMalloc(elementType, arraySize, "list.data");

    // Store data pointer
    llvm::Value *dataStorePtr = builder.CreateStructGEP(listType, listAlloc, 1, "list.data_ptr");
    builder.CreateStore(dataPtr, dataStorePtr);

    // Store first element
    llvm::Value *elementPtr = builder.CreateGEP(elementType, dataPtr,
                                                llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 0),
                                                "list.element");
    builder.CreateStore(firstElement, elementPtr);

    // Process rest of elements
    for (size_t i = 1; i < expr->elements.size(); ++i)
    {
        expr->elements[i]->accept(*this);
        llvm::Value *element = lastValue;
        if (!element)
            return;

        // Validate element type
        if (element->getType() != elementType)
        {
            errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                     "List elements must have the same type",
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            return;
        }

        // Store element
        elementPtr = builder.CreateGEP(elementType, dataPtr,
                                       llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), i),
                                       "list.element");
        builder.CreateStore(element, elementPtr);
    }

    // Return list
    lastValue = listAlloc;
}

void IRGenerator::createEmptyList(ast::TypePtr listType)
{
    // Get element type from list type
    llvm::Type *elementType = nullptr;

    if (auto genericType = std::dynamic_pointer_cast<ast::GenericType>(listType))
    {
        if (genericType->name == "list" && !genericType->typeArguments.empty())
        {
            elementType = getLLVMType(genericType->typeArguments[0]);
        }
    }

    if (!elementType)
    {
        // Default to int if unable to determine
        elementType = llvm::Type::getInt64Ty(context);
    }

    // Create list struct type: { int64_t length, elementType* data }
    std::vector<llvm::Type *> listFields = {
        llvm::Type::getInt64Ty(context),          // length
        llvm::PointerType::getUnqual(elementType) // data
    };
    llvm::StructType *listType = llvm::StructType::get(context, listFields);

    // Allocate list struct
    llvm::AllocaInst *listAlloc = builder.CreateAlloca(listType, nullptr, "empty_list");

    // Set length to 0
    llvm::Value *lengthPtr = builder.CreateStructGEP(listType, listAlloc, 0, "list.length");
    builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 0), lengthPtr);

    // Set data pointer to null
    llvm::Value *dataStorePtr = builder.CreateStructGEP(listType, listAlloc, 1, "list.data_ptr");
    builder.CreateStore(llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(elementType)), dataStorePtr);

    // Return list
    lastValue = listAlloc;
}

void IRGenerator::visitDictionaryExpr(ast::DictionaryExpr *expr)
{
    if (expr->keys.empty())
    {
        // Create empty dictionary
        createEmptyDictionary(expr->type);
        return;
    }

    // Evaluate the first key and value to determine types
    expr->keys[0]->accept(*this);
    llvm::Value *firstKey = lastValue;
    if (!firstKey)
        return;

    expr->values[0]->accept(*this);
    llvm::Value *firstValue = lastValue;
    if (!firstValue)
        return;

    // Get key and value types
    llvm::Type *keyType = firstKey->getType();
    llvm::Type *valueType = firstValue->getType();

    // For now, we'll implement dictionary as a simple struct with keys and values arrays
    // { int64_t size, keyType* keys, valueType* values }
    std::vector<llvm::Type *> dictFields = {
        llvm::Type::getInt64Ty(context),        // size
        llvm::PointerType::getUnqual(keyType),  // keys
        llvm::PointerType::getUnqual(valueType) // values
    };
    llvm::StructType *dictType = llvm::StructType::get(context, dictFields);

    // Allocate dictionary struct
    llvm::AllocaInst *dictAlloc = builder.CreateAlloca(dictType, nullptr, "dict");

    // Set size
    llvm::Value *sizePtr = builder.CreateStructGEP(dictType, dictAlloc, 0, "dict.size");
    builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), expr->keys.size()), sizePtr);

    // Allocate arrays for keys and values
    llvm::Value *arraySize = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), expr->keys.size());
    llvm::Value *keysPtr = builder.CreateArrayMalloc(keyType, arraySize, "dict.keys");
    llvm::Value *valuesPtr = builder.CreateArrayMalloc(valueType, arraySize, "dict.values");

    // Store pointers
    llvm::Value *keysStorePtr = builder.CreateStructGEP(dictType, dictAlloc, 1, "dict.keys_ptr");
    builder.CreateStore(keysPtr, keysStorePtr);

    llvm::Value *valuesStorePtr = builder.CreateStructGEP(dictType, dictAlloc, 2, "dict.values_ptr");
    builder.CreateStore(valuesPtr, valuesStorePtr);

    // Store first key-value pair
    llvm::Value *keyPtr = builder.CreateGEP(keyType, keysPtr,
                                            llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 0),
                                            "dict.key");
    builder.CreateStore(firstKey, keyPtr);

    llvm::Value *valuePtr = builder.CreateGEP(valueType, valuesPtr,
                                              llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 0),
                                              "dict.value");
    builder.CreateStore(firstValue, valuePtr);

    // Process rest of key-value pairs
    for (size_t i = 1; i < expr->keys.size(); ++i)
    {
        expr->keys[i]->accept(*this);
        llvm::Value *key = lastValue;
        if (!key)
            return;

        expr->values[i]->accept(*this);
        llvm::Value *value = lastValue;
        if (!value)
            return;

        // Validate types
        if (key->getType() != keyType || value->getType() != valueType)
        {
            errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                     "Dictionary keys and values must have consistent types",
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            return;
        }

        // Store key-value pair
        keyPtr = builder.CreateGEP(keyType, keysPtr,
                                   llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), i),
                                   "dict.key");
        builder.CreateStore(key, keyPtr);

        valuePtr = builder.CreateGEP(valueType, valuesPtr,
                                     llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), i),
                                     "dict.value");
        builder.CreateStore(value, valuePtr);
    }

    // Return dictionary
    lastValue = dictAlloc;
}

void IRGenerator::createEmptyDictionary(ast::TypePtr dictType)
{
    // Get key and value types from dictionary type
    llvm::Type *keyType = nullptr;
    llvm::Type *valueType = nullptr;

    if (auto genericType = std::dynamic_pointer_cast<ast::GenericType>(dictType))
    {
        if (genericType->name == "dict" && genericType->typeArguments.size() >= 2)
        {
            keyType = getLLVMType(genericType->typeArguments[0]);
            valueType = getLLVMType(genericType->typeArguments[1]);
        }
    }

    if (!keyType || !valueType)
    {
        // Default to string->int if unable to determine
        keyType = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(context)); // string
        valueType = llvm::Type::getInt64Ty(context);                            // int
    }

    // Create dictionary struct type
    std::vector<llvm::Type *> dictFields = {
        llvm::Type::getInt64Ty(context),        // size
        llvm::PointerType::getUnqual(keyType),  // keys
        llvm::PointerType::getUnqual(valueType) // values
    };
    llvm::StructType *dictType = llvm::StructType::get(context, dictFields);

    // Allocate dictionary struct
    llvm::AllocaInst *dictAlloc = builder.CreateAlloca(dictType, nullptr, "empty_dict");

    // Set size to 0
    llvm::Value *sizePtr = builder.CreateStructGEP(dictType, dictAlloc, 0, "dict.size");
    builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 0), sizePtr);

    // Set pointers to null
    llvm::Value *keysStorePtr = builder.CreateStructGEP(dictType, dictAlloc, 1, "dict.keys_ptr");
    builder.CreateStore(llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(keyType)), keysStorePtr);

    llvm::Value *valuesStorePtr = builder.CreateStructGEP(dictType, dictAlloc, 2, "dict.values_ptr");
    builder.CreateStore(llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(valueType)), valuesStorePtr);

    // Return dictionary
    lastValue = dictAlloc;
}
