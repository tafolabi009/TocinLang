#include "ir_generator.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/TargetSelect.h"
#include <fstream>
#include <stdexcept>

IRGenerator::IRGenerator()
    : context(std::make_unique<llvm::LLVMContext>()),
      module(std::make_unique<llvm::Module>("TocinModule", *context)),
      builder(std::make_unique<llvm::IRBuilder<>>(*context)),
      currentSymbolTable(new SymbolTable()),
      currentFunction(nullptr),
      valueStack()
{
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();
}

void IRGenerator::generate(std::shared_ptr<ast::Statement> ast, const std::string &outputFile)
{
    generateGlobalInitialization();
    generateStandardLibraryFunctions();
    ast->accept(*this);

    // Optimization passes
    {
        llvm::PassBuilder passBuilder;
        llvm::LoopAnalysisManager loopAM;
        llvm::FunctionAnalysisManager functionAM;
        llvm::CGSCCAnalysisManager cgsccAM;
        llvm::ModuleAnalysisManager moduleAM;
        passBuilder.registerModuleAnalyses(moduleAM);
        passBuilder.registerCGSCCAnalyses(cgsccAM);
        passBuilder.registerFunctionAnalyses(functionAM);
        passBuilder.registerLoopAnalyses(loopAM);
        passBuilder.crossRegisterProxies(loopAM, functionAM, cgsccAM, moduleAM);

        llvm::ModulePassManager modulePM = passBuilder.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O2);
        modulePM.run(*module, moduleAM);
    }

    if (!verifyAndWriteOutput(outputFile))
    {
        throw std::runtime_error("Failed to verify and write LLVM IR output");
    }
}

llvm::Type *IRGenerator::toLLVMType(std::shared_ptr<ast::Type> type)
{
    std::string typeName = type->toString();
    if (typeName == "int")
        return llvm::Type::getInt64Ty(*context);
    else if (typeName == "float64")
        return llvm::Type::getDoubleTy(*context);
    else if (typeName == "bool")
        return llvm::Type::getInt1Ty(*context);
    else if (typeName == "string")
        return llvm::PointerType::get(llvm::Type::getInt8Ty(*context), 0);
    else if (typeName == "None")
        return llvm::Type::getVoidTy(*context);
    return llvm::Type::getInt64Ty(*context);
}

void IRGenerator::visitLiteralExpr(ast::LiteralExpr *expr)
{
    llvm::Value *value = nullptr;
    switch (expr->literalType)
    {
    case ast::LiteralExpr::LiteralType::INTEGER:
        value = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context), std::stoll(expr->value));
        break;
    case ast::LiteralExpr::LiteralType::FLOAT:
        value = llvm::ConstantFP::get(llvm::Type::getDoubleTy(*context), std::stod(expr->value));
        break;
    case ast::LiteralExpr::LiteralType::STRING:
    {
        llvm::GlobalVariable *str = builder->CreateGlobalString(expr->value, "str");
        value = builder->CreateConstGEP2_32(str->getValueType(), str, 0, 0);
        break;
    }
    case ast::LiteralExpr::LiteralType::BOOLEAN:
        value = llvm::ConstantInt::get(llvm::Type::getInt1Ty(*context), (expr->value == "True") ? 1 : 0);
        break;
    case ast::LiteralExpr::LiteralType::NIL:
        value = llvm::Constant::getNullValue(llvm::Type::getInt64Ty(*context));
        break;
    default:
        throw std::runtime_error("Unsupported literal type");
    }
    pushValue(value);
}

void IRGenerator::visitCallExpr(ast::CallExpr *expr)
{
    expr->callee->accept(*this);
    llvm::Value *calleeValue = popValue();

    llvm::Function *calleeFunc = llvm::dyn_cast<llvm::Function>(calleeValue);
    if (!calleeFunc)
        throw std::runtime_error("Callee is not a function");

    llvm::FunctionCallee callee(calleeFunc->getFunctionType(), calleeFunc);

    std::vector<llvm::Value *> args;
    for (auto &arg : expr->arguments)
    {
        arg->accept(*this);
        args.push_back(popValue());
    }

    llvm::Value *call = builder->CreateCall(callee, args, "calltmp");
    pushValue(call);
}

void IRGenerator::visitListExpr(ast::ListExpr *expr)
{
    std::vector<llvm::Constant *> constElements;
    for (auto &element : expr->elements)
    {
        element->accept(*this);
        llvm::Value *val = popValue();
        llvm::Constant *constVal = llvm::dyn_cast<llvm::Constant>(val);

        if (!constVal)
            throw std::runtime_error("List element must be a constant");

        constElements.push_back(constVal);
    }
    llvm::ArrayType *arrayType = llvm::ArrayType::get(llvm::Type::getInt64Ty(*context), constElements.size());
    llvm::Constant *arrayConst = llvm::ConstantArray::get(arrayType, llvm::ArrayRef<llvm::Constant *>(constElements));
    pushValue(arrayConst);
}

void IRGenerator::visitIfStmt(ast::IfStmt *stmt)
{
    stmt->condition->accept(*this);
    llvm::Value *condVal = popValue();

    llvm::Function *func = builder->GetInsertBlock()->getParent();

    llvm::BasicBlock *thenBB = llvm::BasicBlock::Create(*context, "then", func);
    llvm::BasicBlock *elseBB = llvm::BasicBlock::Create(*context, "else", func);
    llvm::BasicBlock *mergeBB = llvm::BasicBlock::Create(*context, "ifcont", func);

    builder->CreateCondBr(condVal, thenBB, elseBB);

    builder->SetInsertPoint(thenBB);
    stmt->thenBranch->accept(*this);
    if (!builder->GetInsertBlock()->getTerminator())
        builder->CreateBr(mergeBB);

    builder->SetInsertPoint(elseBB);
    if (stmt->elseBranch)
        stmt->elseBranch->accept(*this);
    if (!builder->GetInsertBlock()->getTerminator())
        builder->CreateBr(mergeBB);

    builder->SetInsertPoint(mergeBB);
}

void IRGenerator::visitWhileStmt(ast::WhileStmt *stmt)
{
    llvm::Function *func = builder->GetInsertBlock()->getParent();

    llvm::BasicBlock *condBB = llvm::BasicBlock::Create(*context, "whilecond", func);
    llvm::BasicBlock *loopBB = llvm::BasicBlock::Create(*context, "whilebody", func);
    llvm::BasicBlock *afterBB = llvm::BasicBlock::Create(*context, "whileafter", func);

    builder->CreateBr(condBB);

    builder->SetInsertPoint(condBB);
    stmt->condition->accept(*this);
    llvm::Value *condVal = popValue();
    builder->CreateCondBr(condVal, loopBB, afterBB);

    builder->SetInsertPoint(loopBB);
    stmt->body->accept(*this);
    if (!builder->GetInsertBlock()->getTerminator())
        builder->CreateBr(condBB);

    builder->SetInsertPoint(afterBB);
}

void IRGenerator::visitForStmt(ast::ForStmt *stmt)
{
    throw std::runtime_error("For loops not implemented in IRGenerator");
}

void IRGenerator::visitFunctionStmt(ast::FunctionStmt *stmt)
{
    std::vector<llvm::Type *> paramTypes;
    for (auto &param : stmt->parameters)
    {
        paramTypes.push_back(toLLVMType(param.type));
    }
    llvm::Type *retType = toLLVMType(stmt->returnType);
    llvm::FunctionType *funcType = llvm::FunctionType::get(retType, paramTypes, false);
    llvm::Function *func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, stmt->name, module.get());
    currentFunction = func;

    llvm::BasicBlock *entry = llvm::BasicBlock::Create(*context, "entry", func);
    builder->SetInsertPoint(entry);
    beginScope();

    unsigned idx = 0;
    for (auto &arg : func->args())
    {
        std::string paramName = stmt->parameters[idx].name;
        arg.setName(paramName);
        llvm::AllocaInst *alloca = builder->CreateAlloca(arg.getType(), nullptr, paramName);
        builder->CreateStore(&arg, alloca);
        currentSymbolTable->define(paramName, alloca);
        idx++;
    }

    stmt->body->accept(*this);

    if (retType->isVoidTy() && !builder->GetInsertBlock()->getTerminator())
    {
        builder->CreateRetVoid();
    }
    else if (!retType->isVoidTy() && !builder->GetInsertBlock()->getTerminator())
    {
        throw std::runtime_error("Function '" + stmt->name + "' must return a value");
    }

    endScope();
    currentFunction = nullptr;
}

void IRGenerator::visitReturnStmt(ast::ReturnStmt *stmt)
{
    if (currentFunction == nullptr)
    {
        throw std::runtime_error("Return statement outside of function");
    }

    llvm::Type *returnType = currentFunction->getReturnType();

    if (returnType->isVoidTy())
    {
        if (stmt->value)
        {
            throw std::runtime_error("Cannot return a value from a void function");
        }
        builder->CreateRetVoid();
        return;
    }

    if (!stmt->value)
    {
        throw std::runtime_error("Non-void function must return a value");
    }

    stmt->value->accept(*this);
    llvm::Value *retVal = popValue();

    if (retVal->getType() != returnType)
    {
        unsigned castOpcode = llvm::CastInst::getCastOpcode(retVal, false, returnType, false);
        retVal = builder->CreateCast(static_cast<llvm::Instruction::CastOps>(castOpcode), retVal, returnType, "cast");
    }

    builder->CreateRet(retVal);
}

// Add these implementations near the top of ir_generator.cpp, after the constructor

void IRGenerator::visitBinaryExpr(ast::BinaryExpr *expr)
{
    throw std::runtime_error("Binary expressions not implemented in IRGenerator");
}

void IRGenerator::visitGroupingExpr(ast::GroupingExpr *expr)
{
    throw std::runtime_error("Grouping expressions not implemented in IRGenerator");
}

void IRGenerator::visitUnaryExpr(ast::UnaryExpr *expr)
{
    throw std::runtime_error("Unary expressions not implemented in IRGenerator");
}

void IRGenerator::visitVariableExpr(ast::VariableExpr *expr)
{
    throw std::runtime_error("Variable expressions not implemented in IRGenerator");
}

void IRGenerator::visitAssignExpr(ast::AssignExpr *expr)
{
    throw std::runtime_error("Assign expressions not implemented in IRGenerator");
}

void IRGenerator::visitGetExpr(ast::GetExpr *expr)
{
    throw std::runtime_error("Get expressions not implemented in IRGenerator");
}

void IRGenerator::visitSetExpr(ast::SetExpr *expr)
{
    throw std::runtime_error("Set expressions not implemented in IRGenerator");
}

void IRGenerator::visitDictionaryExpr(ast::DictionaryExpr *expr)
{
    throw std::runtime_error("Dictionary expressions not implemented in IRGenerator");
}

void IRGenerator::visitLambdaExpr(ast::LambdaExpr *expr)
{
    throw std::runtime_error("Lambda expressions not implemented in IRGenerator");
}

void IRGenerator::visitExpressionStmt(ast::ExpressionStmt *stmt)
{
    stmt->expression->accept(*this);
    popValue(); // Consume the result to avoid unused expression warnings
}

void IRGenerator::visitVariableStmt(ast::VariableStmt *stmt)
{
    stmt->->accept(*this);
    popValue(); // Consume the result to avoid unused expression warnings
}

void IRGenerator::visitBlockStmt(ast::BlockStmt *stmt)
{
    throw std::runtime_error("Block statements not implemented in IRGenerator");
}

void IRGenerator::visitClassStmt(ast::ClassStmt *stmt)
{
    throw std::runtime_error("Class statements not implemented in IRGenerator");
}

void IRGenerator::visitImportStmt(ast::ImportStmt *stmt)
{
    // Imports are handled at a higher level.
}

void IRGenerator::visitMatchStmt(ast::MatchStmt *stmt)
{
    throw std::runtime_error("Match statements not implemented in IRGenerator");
}

// ---------------------------------------------------------------------------
// Symbol Table and Value Stack Management
// ---------------------------------------------------------------------------

void IRGenerator::beginScope()
{
    SymbolTable *newScope = new SymbolTable(currentSymbolTable);
    currentSymbolTable = newScope;
}

void IRGenerator::endScope()
{
    // Changed from 'enclosing' to 'parent' to match the SymbolTable member.
    SymbolTable *parentScope = currentSymbolTable->parent;
    delete currentSymbolTable;
    currentSymbolTable = parentScope;
}

void IRGenerator::pushValue(llvm::Value *value)
{
    valueStack.push(value);
}

llvm::Value *IRGenerator::popValue()
{
    if (valueStack.empty())
    {
        throw std::runtime_error("Value stack underflow");
    }
    llvm::Value *value = valueStack.top();
    valueStack.pop();
    return value;
}

void IRGenerator::generateGlobalInitialization()
{
    // Insert global variable initialization here if needed.
}

void IRGenerator::generateStandardLibraryFunctions()
{
    std::vector<llvm::Type *> printArgs;
    printArgs.push_back(llvm::PointerType::get(llvm::Type::getInt8Ty(*context), 0));
    llvm::FunctionType *printFuncType = llvm::FunctionType::get(
        llvm::Type::getVoidTy(*context), printArgs, false);
    llvm::Function::Create(printFuncType, llvm::Function::ExternalLinkage,
                           "print", module.get());
}

bool IRGenerator::verifyAndWriteOutput(const std::string &outputFile)
{
    std::string errStr;
    llvm::raw_string_ostream errStream(errStr);
    if (llvm::verifyModule(*module, &errStream))
    {
        errStream.flush();
        std::cerr << "Module verification error: " << errStr << std::endl;
        return false;
    }

    if (outputFile.empty())
    {
        // Print generated IR to stdout
        module->print(llvm::outs(), nullptr);
        return true;
    }
    else
    {
        std::error_code EC;
        llvm::raw_fd_ostream dest(outputFile, EC, llvm::sys::fs::OF_None);
        if (EC)
        {
            std::cerr << "Could not open file: " << EC.message() << std::endl;
            return false;
        }
        module->print(dest, nullptr);
        return true;
    }
}
