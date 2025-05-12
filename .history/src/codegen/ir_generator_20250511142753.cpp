// Remove this function as it's no longer needed and references a non-existent type
// llvm::Type *IRGenerator::getLLVMType(std::shared_ptr<type::Type> tocinType)
// {
//     ...
// }

// Remove visitPrintStmt implementation
// void IRGenerator::visitPrintStmt(ast::PrintStmt *stmt)
// {
//     ...
// }

// Remove visitVarStmt implementation
// void IRGenerator::visitVarStmt(ast::VarStmt *stmt)
// {
//     ...
// }

// Add implementation for visitVariableStmt
void IRGenerator::visitVariableStmt(ast::VariableStmt *stmt)
{
    errorHandler.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                             "Variable statement not fully implemented",
                             "", 0, 0, error::ErrorSeverity::ERROR);
}

// Add implementation for visitImportStmt
void IRGenerator::visitImportStmt(ast::ImportStmt *stmt)
{
    errorHandler.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                             "Import statement not implemented",
                             "", 0, 0, error::ErrorSeverity::ERROR);
}

// Add implementation for visitListExpr
void IRGenerator::visitListExpr(ast::ListExpr *expr)
{
    errorHandler.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                             "List expression not implemented",
                             "", 0, 0, error::ErrorSeverity::ERROR);
}

// Add implementation for visitDictionaryExpr
void IRGenerator::visitDictionaryExpr(ast::DictionaryExpr *expr)
{
    errorHandler.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                             "Dictionary expression not implemented",
                             "", 0, 0, error::ErrorSeverity::ERROR);
}

// Add implementation for visitAwaitExpr
void IRGenerator::visitAwaitExpr(ast::AwaitExpr *expr)
{
    errorHandler.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                             "Await expression not implemented",
                             "", 0, 0, error::ErrorSeverity::ERROR);
}

// Remove these as they're not in the AST anymore
// void IRGenerator::visitLogicalExpr(ast::LogicalExpr *expr)
// {
//     ...
// }

// void IRGenerator::visitThisExpr(ast::ThisExpr *expr)
// {
//     ...
// }

// void IRGenerator::visitSuperExpr(ast::SuperExpr *expr)
// {
//     ...
// }

// void IRGenerator::visitArrayExpr(ast::ArrayExpr *expr)
// {
//     ...
// }

// void IRGenerator::visitIndexExpr(ast::IndexExpr *expr)
// {
//     ...
// }

// void IRGenerator::visitOptionExpr(ast::OptionExpr *expr)
// {
//     ...
// }

// void IRGenerator::visitResultExpr(ast::ResultExpr *expr)
// {
//     ...
// }

// Fix the getInt8PtrTy call in getLLVMType method
llvm::Type *IRGenerator::getLLVMType(ast::TypePtr type)
{
    if (!type)
    {
        errorHandler.reportError(error::ErrorCode::C003_TYPECHECK_ERROR,
                                 "Null type passed to getLLVMType",
                                 "", 0, 0, error::ErrorSeverity::FATAL);
        return nullptr;
    }

    std::string typeName = type->toString();

    // Handle basic types
    if (typeName == "int")
    {
        return llvm::Type::getInt64Ty(context);
    }
    else if (typeName == "float" || typeName == "float64")
    {
        return llvm::Type::getDoubleTy(context);
    }
    else if (typeName == "float32")
    {
        return llvm::Type::getFloatTy(context);
    }
    else if (typeName == "bool")
    {
        return llvm::Type::getInt1Ty(context);
    }
    else if (typeName == "string")
    {
        // Fix: Use PointerType::get with Int8Ty instead of getInt8PtrTy
        return llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0);
    }
    else if (typeName == "void")
    {
        return llvm::Type::getVoidTy(context);
    }

    // ... rest of method unchanged
}

// Fix the getInt8PtrTy call in declareStdLibFunctions method
void IRGenerator::declareStdLibFunctions()
{
    // Get common LLVM types
    llvm::Type *voidTy = llvm::Type::getVoidTy(context);
    llvm::Type *i64Ty = llvm::Type::getInt64Ty(context);
    llvm::Type *doubleTy = llvm::Type::getDoubleTy(context);
    // Fix: Use PointerType::get with Int8Ty instead of getInt8PtrTy
    llvm::Type *i8PtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0);
    llvm::Type *boolTy = llvm::Type::getInt1Ty(context);

    // ... rest of method unchanged
}

// Fix the struct creation in getLLVMType method for dictionaries
// Find this code in getLLVMType:
// else if (genericType->name == "dict")
// {
//     // Just a placeholder for now - dictionaries need a more complex implementation
//     return llvm::StructType::create(context, "dict", false);
// }

// And replace it with:
else if (genericType->name == "dict")
{
    // Just a placeholder for now - dictionaries need a more complex implementation
    return llvm::StructType::create(context, "dict");
}

// Fix the CreateGEP call in visitBinaryExpr method
void IRGenerator::visitBinaryExpr(ast::BinaryExpr *expr)
{
    // ... existing code ...

    // Locate any use of CreateGEP that's causing errors and fix it
    // For example, if you have:
    // lastValue = builder.CreateGEP(left, right, "ptradd");

    // Replace it with:
    // lastValue = builder.CreateGEP(llvm::Type::getInt8Ty(context), left, right, "ptradd");

    // ... existing code ...
}

// ... existing code ...
