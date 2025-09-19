#include "compiler.h"
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Scalar/GVN.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/FileSystem.h>
#include "../llvm_shim.h"
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/ADT/APFloat.h>
#include <llvm/ADT/STLExtras.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>
#include <memory>
#include <iostream>

namespace compiler
{

    Compiler::Compiler(error::ErrorHandler &errorHandler)
        : errorHandler(errorHandler), compilationContext("<unknown>")
    {
        // Initialize the LLVM context
        context = std::make_unique<llvm::LLVMContext>();

        // Initialize LLVM targets for code generation
        initializeLLVMTargets();
    }

    void Compiler::initializeLLVMTargets()
    {
        // Initialize all targets for cross-compilation support
        llvm::InitializeAllTargetInfos();
        llvm::InitializeAllTargets();
        llvm::InitializeAllTargetMCs();
        llvm::InitializeAllAsmParsers();
        llvm::InitializeAllAsmPrinters();
    }

    bool Compiler::compile(const std::string &source, const std::string &filename,
                           const CompilationOptions &options)
    {
        // Create a new LLVM module
        module = std::make_unique<llvm::Module>(filename, *context);

        // Perform lexical analysis
        lexer::Lexer lexer(source, filename);
        std::vector<lexer::Token> tokens = lexer.tokenize();

        if (errorHandler.hasErrors())
        {
            std::cerr << "Lexical analysis failed. See errors.\n";
            return false;
        }

        // Parse the tokens into an AST
        parser::Parser parser(tokens);
        ast::StmtPtr ast = parser.parse();

        if (!ast || errorHandler.hasErrors())
        {
            std::cerr << "Parsing failed. See errors.\n";
            return false;
        }

        if (options.serializeAst)
        {
            // Simplified: Just print that it would serialize the AST
            std::cout << "Would serialize AST to XML (not implemented).\n";
        }

        // Type check the AST - pass the required compilation context
        type_checker::TypeChecker typeChecker(errorHandler, compilationContext);
        typeChecker.check(ast);

        if (errorHandler.hasErrors())
        {
            std::cerr << "Type checking failed. See errors.\n";
            return false;
        }

        // Generate LLVM IR - we need to pass the full required arguments
        codegen::IRGenerator irGenerator(*context, std::move(module), errorHandler);
        module = irGenerator.generate(ast);

        if (!module || errorHandler.hasErrors())
        {
            std::cerr << "IR generation failed. See errors.\n";
            return false;
        }

        // Optimize the generated code
        if (options.optimize)
        {
            if (!optimizeModule(options.optimizationLevel))
            {
                std::cerr << "Optimization failed. See errors.\n";
                return false;
            }
        }

        // Dump IR if requested
        if (options.dumpIR)
        {
            std::string ir;
            llvm::raw_string_ostream irStream(ir);
            module->print(irStream, nullptr);
            std::cout << ir;
        }

        // Output to file if requested
        if (!options.outputFile.empty())
        {
            if (!outputToFile(options.outputFile, options.generateObject, options.generateAssembly))
            {
                std::cerr << "Failed to output to file. See errors.\n";
                return false;
            }
        }

        return true;
    }

    std::unique_ptr<llvm::Module> Compiler::getModule()
    {
        return std::move(module);
    }

    int Compiler::executeJIT()
    {
        if (!module)
        {
            errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                     "No module to execute",
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            return 1;
        }

        // Create the execution engine
        std::string errStr;
        executionEngine = std::unique_ptr<llvm::ExecutionEngine>(
            llvm::EngineBuilder(std::move(module))
                .setErrorStr(&errStr)
                .setEngineKind(llvm::EngineKind::JIT)
                .create());

        if (!executionEngine)
        {
            errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                     "Failed to create execution engine: " + errStr,
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            return 1;
        }

        // Look up the main function
        auto mainFunc = executionEngine->FindFunctionNamed("main");
        if (!mainFunc)
        {
            errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                     "No main function found",
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            return 1;
        }

        // Execute the main function
        executionEngine->finalizeObject();
        typedef int (*MainFuncType)();
        auto mainFuncPtr = (MainFuncType)executionEngine->getPointerToFunction(mainFunc);
        return mainFuncPtr();
    }

    bool Compiler::optimizeModule(int level)
    {
        if (!module)
        {
            errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                     "No module to optimize",
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            return false;
        }

        // Create a function pass manager
        auto passManager = std::make_unique<llvm::legacy::FunctionPassManager>(module.get());

        // Add optimization passes based on level
        if (level >= 1)
        {
            // Basic optimizations - use available functions
            // Replaced createPromoteMemoryToRegisterPass with available functions
            passManager->add(llvm::createInstructionCombiningPass());
            passManager->add(llvm::createReassociatePass());
        }

        if (level >= 2)
        {
            // Intermediate optimizations
            passManager->add(llvm::createGVNPass());
            passManager->add(llvm::createCFGSimplificationPass());
        }

        if (level >= 3)
        {
            // Advanced optimizations
            // Add more aggressive optimizations here
        }

        // Initialize and run the pass manager
        passManager->doInitialization();

        // Run the optimizations on each function in the module
        for (auto &func : *module)
        {
            passManager->run(func);
        }

        passManager->doFinalization();

        return true;
    }

    std::unique_ptr<llvm::TargetMachine> Compiler::createTargetMachine()
    {
        // Get the target triple for the host
        auto targetTriple = llvm::sys::getDefaultTargetTriple();

        // Look up the target
        std::string error;
        auto target = llvm::TargetRegistry::lookupTarget(targetTriple, error);

        if (!target)
        {
            errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                     "Failed to lookup target: " + error,
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            return nullptr;
        }

        // Create a target machine
        llvm::TargetOptions opt;
        auto relocationModel = llvm::Optional<llvm::Reloc::Model>();
        auto targetMachine = target->createTargetMachine(
            targetTriple,
            "generic", // CPU
            "",        // Features
            opt,
            relocationModel);

        if (!targetMachine)
        {
            errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                     "Failed to create target machine",
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            return nullptr;
        }

        return std::unique_ptr<llvm::TargetMachine>(targetMachine);
    }

    bool Compiler::outputToFile(const std::string &outputFile, bool generateObject, bool generateAssembly)
    {
        if (!module)
        {
            errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                     "No module to output",
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            return false;
        }

        // Create the target machine
        auto targetMachine = createTargetMachine();
        if (!targetMachine)
        {
            return false;
        }

        // Set the module's target triple
        module->setTargetTriple(targetMachine->getTargetTriple().getTriple());

        // Set the data layout
        module->setDataLayout(targetMachine->createDataLayout());

        // Create the output file
        std::error_code EC;
        llvm::raw_fd_ostream dest(outputFile, EC, llvm::sys::fs::OF_None);

        if (EC)
        {
            errorHandler.reportError(error::ErrorCode::I004_WRITE_ERROR,
                                     "Could not open output file: " + EC.message(),
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            return false;
        }

        // Configure the output type
        llvm::CodeGenFileType fileType;
        if (generateObject)
        {
            fileType = llvm::CGFT_ObjectFile;
        }
        else if (generateAssembly)
        {
            fileType = llvm::CGFT_AssemblyFile;
        }
        else
        {
            // Just dump the LLVM IR
            module->print(dest, nullptr);
            return true;
        }

        // Add passes to the pass manager
        llvm::legacy::PassManager pass;
        if (targetMachine->addPassesToEmitFile(pass, dest, nullptr, fileType))
        {
            errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                     "Target machine can't emit a file of this type",
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            return false;
        }

        // Generate the code
        pass.run(*module);
        dest.flush();

        return true;
    }

} // namespace compiler
