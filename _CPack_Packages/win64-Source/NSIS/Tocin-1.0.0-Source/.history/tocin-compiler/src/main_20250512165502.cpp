#include <iostream>
#include <memory>

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

int main(int argc, char *argv[])
{
    std::cout << "Tocin Compiler - Simplified Version" << std::endl;

    // Create LLVM context and module
    llvm::LLVMContext context;
    auto module = std::make_unique<llvm::Module>("tocin_module", context);

    // Create a simple main function
    llvm::FunctionType *mainType = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(context), false);
    llvm::Function *mainFunction = llvm::Function::Create(
        mainType, llvm::Function::ExternalLinkage, "main", module.get());

    // Create basic block for the main function
    llvm::BasicBlock *block = llvm::BasicBlock::Create(context, "entry", mainFunction);
    llvm::IRBuilder<> builder(context);
    builder.SetInsertPoint(block);

    // Return 0 from main
    builder.CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0));

    // Verify the module
    std::string error;
    llvm::raw_string_ostream errorStream(error);
    if (llvm::verifyModule(*module, &errorStream))
    {
        std::cerr << "Module verification failed: " << errorStream.str() << std::endl;
        return 1;
    }

    // Print the generated IR
    std::string ir;
    llvm::raw_string_ostream irStream(ir);
    module->print(irStream, nullptr);
    std::cout << "\nGenerated LLVM IR:\n"
              << irStream.str() << std::endl;

    std::cout << "Compilation successful!" << std::endl;
    return 0;
}
