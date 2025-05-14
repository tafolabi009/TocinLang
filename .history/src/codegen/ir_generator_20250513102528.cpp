// ... existing code ...

// In visitLiteralExpr - replace CreateGlobalStringPtr with CreateGlobalString
lastValue = builder.CreateGlobalString(processedStr, "str");

// ... existing code ...

// Fix CreateMalloc calls in visitDictionaryExpr
llvm::Value *keysPtr = builder.CreateMalloc(
    llvm::Type::getInt64Ty(context), // IntPtrTy
    keyType,                         // AllocTy
    arraySize,                       // AllocSize
    nullptr,                         // ArraySize
    nullptr,                         // Function
    "dict.keys");                    // Name

llvm::Value *valuesPtr = builder.CreateMalloc(
    llvm::Type::getInt64Ty(context), // IntPtrTy
    valueType,                       // AllocTy
    arraySize,                       // AllocSize
    nullptr,                         // ArraySize
    nullptr,                         // Function
    "dict.values");                  // Name

// ... existing code ...

// Fix getElementType in visitSetExpr
// Replace:
pointedType = ptrType->getElementType();
// With:
pointedType = ptrType->getPointerElementType();

// And also:
fieldType = ptrType->getElementType();
// With:
fieldType = ptrType->getPointerElementType();

// And also:
base = builder.CreateLoad(ptrType->getPointerElementType(), basePtr);
// With:
base = builder.CreateLoad(ptrType->getPointerElementType(), basePtr);

// ... existing code ...

// Fix generateMethod function to use parameters instead of params
for (const auto &param : method->parameters)
{
    // ... existing code ...
}

// And fix the name.lexeme issue - method->name is already a string
std::string methodName = className + "_" + method->name;

// And fix the params reference
argIt->setName(method->parameters[idx].name);

// And fix the final method->name.lexeme
classMethods[className + "." + method->name] = function;

// ... existing code ...

// Fix 6: Fix the malformed "Handle variable assignment" code at line 2202
// Comment it out properly or rewrite it:
// Handle variable assignment
if (auto varExpr = dynamic_cast<ast::VariableExpr *>(expr->target.get()))
