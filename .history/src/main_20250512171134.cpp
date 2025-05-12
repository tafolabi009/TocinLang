#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <memory>
#include <vector>

// Core LLVM headers
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Host.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Scalar/GVN.h>
#include <llvm/Analysis/LoopAnalysisManager.h>
#include <llvm/Analysis/CGSCCPassManager.h>
#include <llvm/Passes/PassBuilder.h>

// Conditionally include Python
#ifdef WITH_PYTHON
#include <Python.h>
#endif

// Include compiler components
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "ast/ast.h"
#include "type/type_checker.h"
#include "codegen/ir_generator.h"
#include "error/error_handler.h"

/**
 * @brief Simple compiler structure to hold compilation options and handle the compilation process.
 */
class Compiler
{
public:
    Compiler(error::ErrorHandler &errorHandler) : errorHandler(errorHandler) {}

    struct CompilationOptions
    {
        bool dumpIR = false;
        bool optimize = false;
        int optimizationLevel = 2;
        std::string outputFile;
    };

    bool compile(const std::string &source, const std::string &filename,
                 const CompilationOptions &options = CompilationOptions())
    {
        std::cout << "Compiling " << filename << "..." << std::endl;

        // Create a lexer
        lexer::Lexer lexer(source, filename, errorHandler);

        // Tokenize the source
        std::vector<lexer::Token> tokens = lexer.scanTokens();
        if (errorHandler.hasFatalErrors())
        {
            std::cerr << "Lexical analysis failed." << std::endl;
            return false;
        }

        // Create a parser
        parser::Parser parser(tokens, errorHandler);

        // Parse the tokens into an AST
        ast::StmtPtr program = parser.parse();
        if (errorHandler.hasFatalErrors() || !program)
        {
            std::cerr << "Parsing failed." << std::endl;
            return false;
        }

        // Create type checker
        type_checker::TypeChecker typeChecker(errorHandler);

        // Type check the AST
        program->accept(typeChecker);
        if (errorHandler.hasFatalErrors())
        {
            std::cerr << "Type checking failed." << std::endl;
            return false;
        }

        // Create LLVM context and module
        llvm::LLVMContext context;
        auto module = std::make_unique<llvm::Module>(filename, context);

        // Set up target triple for the current machine
        module->setTargetTriple(llvm::sys::getDefaultTargetTriple());

        // Create IR generator
        ir_generator::IRGenerator generator(context, std::move(module), errorHandler);

        // Generate LLVM IR from the AST
        auto generatedModule = generator.generate(program);
        if (errorHandler.hasFatalErrors() || !generatedModule)
        {
            std::cerr << "IR generation failed." << std::endl;
            return false;
        }

        // Apply optimizations if requested
        if (options.optimize && !errorHandler.hasFatalErrors())
        {
            optimizeModule(*generatedModule, options.optimizationLevel);
        }

        // Print the IR if requested
        if (options.dumpIR && !errorHandler.hasFatalErrors())
        {
            std::string ir;
            llvm::raw_string_ostream irStream(ir);
            generatedModule->print(irStream, nullptr);
            std::cout << "\nGenerated LLVM IR:\n"
                      << irStream.str() << std::endl;
        }

        // Write IR to file if output file is specified
        if (!options.outputFile.empty() && !errorHandler.hasFatalErrors())
        {
            std::error_code EC;
            llvm::raw_fd_ostream outFile(options.outputFile, EC, llvm::sys::fs::OF_None);
            if (EC)
            {
                errorHandler.reportError(error::ErrorCode::I001_FILE_NOT_FOUND,
                                         "Could not open output file: " + options.outputFile);
                return false;
            }
            generatedModule->print(outFile, nullptr);
            std::cout << "IR written to " << options.outputFile << std::endl;
        }

        if (!errorHandler.hasFatalErrors())
        {
            std::cout << "Compilation successful!" << std::endl;
            return true;
        }
        else
        {
            std::cerr << "Compilation failed with errors." << std::endl;
            return false;
        }
    }

private:
    error::ErrorHandler &errorHandler;

    void optimizeModule(llvm::Module &module, int level)
    {
        // Create the analysis managers
        llvm::LoopAnalysisManager LAM;
        llvm::FunctionAnalysisManager FAM;
        llvm::CGSCCAnalysisManager CGAM;
        llvm::ModuleAnalysisManager MAM;

        // Create the pass builder
        llvm::PassBuilder PB;

        // Register all the basic analyses with the managers
        PB.registerModuleAnalyses(MAM);
        PB.registerCGSCCAnalyses(CGAM);
        PB.registerFunctionAnalyses(FAM);
        PB.registerLoopAnalyses(LAM);
        PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

        // Create optimization pipeline based on optimization level
        llvm::ModulePassManager MPM;
        if (level == 0)
        {
            // No optimizations
        }
        else if (level == 1)
        {
            MPM = PB.buildO1DefaultPipeline();
        }
        else if (level == 2)
        {
            MPM = PB.buildO2DefaultPipeline();
        }
        else if (level == 3)
        {
            MPM = PB.buildO3DefaultPipeline();
        }

        // Run the passes
        MPM.run(module, MAM);
    }
};

/**
 * @brief Displays usage information.
 */
void displayUsage()
{
    std::cout << "Usage: tocin [options] [filename]\n"
              << "Options:\n"
              << "  --help                 Display this help message\n"
              << "  --dump-ir              Dump LLVM IR to stdout\n"
              << "  -O0, -O1, -O2, -O3     Set optimization level (default: -O2)\n"
              << "  -o <file>              Write output to <file>\n"
              << std::endl;
}

/**
 * @brief Simple REPL for interactive compilation.
 */
void runRepl(Compiler &compiler, error::ErrorHandler &errorHandler)
{
    std::string line;
    std::stringstream source;
    Compiler::CompilationOptions options;
    options.dumpIR = true;

    std::cout << "Tocin REPL (type 'exit' to quit, 'clear' to reset)\n> ";

    while (std::getline(std::cin, line))
    {
        if (line == "exit")
            break;
        if (line == "clear")
        {
            source.str("");
            errorHandler.clearErrors();
            std::cout << "> ";
            continue;
        }

        source << line << "\n";
        if (compiler.compile(source.str(), "<repl>", options))
        {
            // Success
        }
        else
        {
            // Clear errors to continue the REPL
            errorHandler.clearErrors();
        }

        std::cout << "> ";
    }
}

/**
 * @brief Main entry point for the Tocin compiler.
 */
int main(int argc, char *argv[])
{
    // Initialize LLVM
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

// Initialize Python if available
#ifdef WITH_PYTHON
    Py_Initialize();
#endif

    // Create error handler
    error::ErrorHandler errorHandler;

    // Create compiler
    Compiler compiler(errorHandler);

    // If no arguments, run REPL
    if (argc == 1)
    {
        runRepl(compiler, errorHandler);
        return 0;
    }

    // Parse command-line arguments
    Compiler::CompilationOptions options;
    std::string filename;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];

        if (arg == "--help")
        {
            displayUsage();
            return 0;
        }
        else if (arg == "--dump-ir")
        {
            options.dumpIR = true;
        }
        else if (arg == "-O0")
        {
            options.optimize = true;
            options.optimizationLevel = 0;
        }
        else if (arg == "-O1")
        {
            options.optimize = true;
            options.optimizationLevel = 1;
        }
        else if (arg == "-O2")
        {
            options.optimize = true;
            options.optimizationLevel = 2;
        }
        else if (arg == "-O3")
        {
            options.optimize = true;
            options.optimizationLevel = 3;
        }
        else if (arg == "-o" && i + 1 < argc)
        {
            options.outputFile = argv[++i];
        }
        else if (arg[0] == '-')
        {
            std::cerr << "Unknown option: " << arg << std::endl;
            displayUsage();
            return 1;
        }
        else
        {
            filename = arg;
        }
    }

    // Check if a filename was provided
    if (filename.empty())
    {
        std::cerr << "Error: No input file specified.\n";
        displayUsage();
        return 1;
    }

    // Read the source file
    std::ifstream file(filename);
    if (!file.is_open())
    {
        errorHandler.reportError(error::ErrorCode::I001_FILE_NOT_FOUND,
                                 "Could not open file: " + filename);
        return 1;
    }

    std::string source((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
    file.close();

    // Compile the source
    if (!compiler.compile(source, filename, options))
    {
        return 1;
    }

// Clean up Python if it was initialized
#ifdef WITH_PYTHON
    Py_Finalize();
#endif

    return 0;
}
