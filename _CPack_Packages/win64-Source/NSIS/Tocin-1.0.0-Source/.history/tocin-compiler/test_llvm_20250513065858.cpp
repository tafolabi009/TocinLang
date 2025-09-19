#include <iostream>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Support/raw_ostream.h>

int main() {
    // Create LLVM context
    llvm::LLVMContext context;
    std::cout << "LLVM context created successfully" << std::endl;
    
    // Create a module
    std::unique_ptr<llvm::Module> module = 
        std::make_unique<llvm::Module>("TestModule", context);
    std::cout << "Module created successfully" << std::endl;
    
    // Create an IR builder
    llvm::IRBuilder<> builder(context);
    std::cout << "IR builder created successfully" << std::endl;
    
    // Create a simple function
    llvm::FunctionType *funcType = 
        llvm::FunctionType::get(llvm::Type::getInt32Ty(context), false);
    llvm::Function *mainFunc = 
        llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "test_function", module.get());
    std::cout << "Function created successfully" << std::endl;
    
    // Create a basic block
    llvm::BasicBlock *entry = llvm::BasicBlock::Create(context, "entry", mainFunc);
    builder.SetInsertPoint(entry);
    std::cout << "Basic block created successfully" << std::endl;
    
    // Create a return instruction
    builder.CreateRet(llvm::ConstantInt::get(context, llvm::APInt(32, 0)));
    std::cout << "Return instruction created successfully" << std::endl;
    
    // Print the module IR to stdout
    std::string ir;
    llvm::raw_string_ostream os(ir);
    os << *module;
    std::cout << "\nGenerated LLVM IR:\n" << ir << std::endl;
    
    std::cout << "LLVM integration test completed successfully!" << std::endl;
    return 0;
} 
