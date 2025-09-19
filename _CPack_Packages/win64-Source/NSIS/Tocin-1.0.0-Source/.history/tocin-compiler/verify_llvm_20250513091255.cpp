#include <iostream>
#include <memory>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Support/raw_ostream.h>

int main() {
    // Report LLVM version
    std::cout << "Using LLVM version: " << LLVM_VERSION_STRING << std::endl;
    std::cout << "Checking opaque pointer support..." << std::endl;
    
    // Initialize LLVM components
    llvm::LLVMContext context;
    std::unique_ptr<llvm::Module> module = std::make_unique<llvm::Module>("llvm_test", context);
    llvm::IRBuilder<> builder(context);
    
    // Create a simple test function
    llvm::FunctionType *funcType = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(context), false);
    llvm::Function *testFunc = llvm::Function::Create(
        funcType, llvm::Function::ExternalLinkage, "test", *module);
    
    // Create a basic block
    llvm::BasicBlock *entryBlock = llvm::BasicBlock::Create(context, "entry", testFunc);
    builder.SetInsertPoint(entryBlock);
    
    // Test opaque pointers (new style in LLVM 15+)
    try {
        // Create opaque pointer (will work in LLVM 15+)
        llvm::Type *opaquePtr = llvm::PointerType::get(context, 0);
        
        // Allocate memory for an integer
        llvm::AllocaInst *intAlloca = builder.CreateAlloca(
            llvm::Type::getInt32Ty(context), nullptr, "intVar");
        
        // Store value
        builder.CreateStore(
            llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 42),
            intAlloca);
            
        // Load value using proper type
        llvm::Value *loadedVal = builder.CreateLoad(
            llvm::Type::getInt32Ty(context), intAlloca, "loadedInt");
        
        // Return value
        builder.CreateRet(loadedVal);
        
        std::cout << "Opaque pointer code generation successful!" << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Error testing opaque pointers: " << e.what() << std::endl;
        return 1;
    }
    
    // Verify the module
    std::string verifyOutput;
    llvm::raw_string_ostream verifyStream(verifyOutput);
    bool verifyError = llvm::verifyModule(*module, &verifyStream);
    
    if (verifyError) {
        std::cerr << "Module verification failed: " << verifyOutput << std::endl;
        return 1;
    }
    
    // Dump the module IR
    std::string output;
    llvm::raw_string_ostream outputStream(output);
    module->print(outputStream, nullptr);
    
    std::cout << "\nGenerated LLVM IR:\n" << output << std::endl;
    std::cout << "LLVM verification successful!" << std::endl;
    
    return 0;
} 
