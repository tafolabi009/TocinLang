#include <iostream>

// Just include one LLVM header to test
#include <llvm/IR/LLVMContext.h>

int main()
{
    // Create a LLVM context
    llvm::LLVMContext context;

    std::cout << "LLVM context created successfully" << std::endl;
    return 0;
}
