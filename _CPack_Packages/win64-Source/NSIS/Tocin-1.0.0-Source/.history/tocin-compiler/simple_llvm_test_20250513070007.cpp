#include <iostream>
#include <llvm/IR/LLVMContext.h>

int main()
{
    std::cout << "Creating LLVM context..." << std::endl;
    llvm::LLVMContext context;
    std::cout << "LLVM context created successfully!" << std::endl;
    return 0;
}
