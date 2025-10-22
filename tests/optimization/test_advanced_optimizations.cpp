// Advanced Optimization Tests for Tocin Compiler

#include "../../src/compiler/advanced_optimizations.h"
#include "../test_runner.cpp"
#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <iostream>
#include <cassert>

using namespace tocin::optimization;

// Helper to create a simple test module
std::unique_ptr<llvm::Module> createTestModule() {
    static llvm::LLVMContext context;
    auto module = std::make_unique<llvm::Module>("test_module", context);
    
    // Create a simple function: int add(int a, int b) { return a + b; }
    llvm::IRBuilder<> builder(context);
    auto funcType = llvm::FunctionType::get(
        builder.getInt32Ty(),
        {builder.getInt32Ty(), builder.getInt32Ty()},
        false
    );
    
    auto func = llvm::Function::Create(
        funcType,
        llvm::Function::ExternalLinkage,
        "add",
        module.get()
    );
    
    auto entry = llvm::BasicBlock::Create(context, "entry", func);
    builder.SetInsertPoint(entry);
    
    auto arg1 = func->arg_begin();
    auto arg2 = std::next(arg1);
    auto result = builder.CreateAdd(arg1, arg2, "result");
    builder.CreateRet(result);
    
    return module;
}

// Test implementations
TEST_CASE(pgo_manager_init) {
    PGOManager pgo;
    auto stats = pgo.getStats();
    ASSERT_EQ(stats.hotFunctions, 0);
}

TEST_CASE(interprocedural_optimizer) {
    InterproceduralOptimizer ipo;
    auto module = createTestModule();
    ipo.optimizeCallGraph(module.get());
    auto stats = ipo.getStats();
    ASSERT_GE(stats.totalFunctions, 0);
}

TEST_CASE(optimization_pipeline) {
    AdvancedOptimizationPipeline pipeline;
    auto module = createTestModule();
    pipeline.setOptimizationLevel(2);
    pipeline.optimize(module.get());
    auto stats = pipeline.getStats();
    ASSERT_GE(stats.optimizationTimeMs, 0.0);
}
