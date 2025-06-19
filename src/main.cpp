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
// Include our LLVM shim header instead of directly including Host.h
#include "llvm_shim.h"
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
// Include the actual IR generator
#include "codegen/ir_generator.h"
#include "error/error_handler.h"
#include "compiler/compilation_context.h"
// Include the feature integration header
#include "type/feature_integration.h"

// FFI Support
#include "ffi/ffi_interface.h"
#include "ffi/ffi_value.h"
#ifdef WITH_PYTHON
#include "ffi/ffi_python.h"
#endif
#include "ffi/ffi_cpp.h"
#include "ffi/ffi_javascript.h"

// Advanced features
#include "type/option_result_types.h"
#include "type/traits.h"
#include "type/ownership.h"
#include "type/null_safety.h"
#include "type/move_semantics.h"
#include "type/extension_functions.h"
#include "runtime/concurrency.h"
#include "runtime/linq.h"

/**
 * @brief Simple compiler structure to hold compilation options and handle the compilation process.
 */
class Compiler
{
public:
    Compiler(error::ErrorHandler &errorHandler)
        : errorHandler(errorHandler), featureManager(errorHandler)
    {
        // Initialize feature manager
        featureManager.initialize();
        
        // Initialize FFI systems
        initializeFFI();
    }

    struct CompilationOptions
    {
        bool dumpIR;
        bool optimize;
        int optimizationLevel;
        std::string outputFile;
        bool enableFFI;
        bool enableConcurrency;
        bool enableAdvancedFeatures;

        CompilationOptions()
            : dumpIR(false), optimize(false), optimizationLevel(2), outputFile(""),
              enableFFI(true), enableConcurrency(true), enableAdvancedFeatures(true) {}
    };

    bool compile(const std::string &source, const std::string &filename,
                 const CompilationOptions &options = CompilationOptions())
    {
        // Lexical analysis
        lexer::Lexer lexer(source, filename, 4);
        std::vector<lexer::Token> tokens = lexer.tokenize();

        if (errorHandler.hasFatalErrors())
        {
            return false;
        }

        // Parsing
        parser::Parser parser(tokens);
        ast::StmtPtr program = parser.parse();

        if (errorHandler.hasFatalErrors() || !program)
        {
            return false;
        }

        // Create compilation context with advanced features
        tocin::compiler::CompilationContext compilationContext(filename);
        // Advanced features are managed by FeatureManager and TypeChecker
        // No need to call enable* methods on compilationContext
        // FFI and concurrency are enabled by default if available

        // Type checking with advanced features
        type_checker::TypeChecker checker(errorHandler, compilationContext, &featureManager);
        checker.check(program);

        if (errorHandler.hasFatalErrors())
        {
            return false;
        }

        // IR generation
        llvm::LLVMContext context;
        auto module = std::make_unique<llvm::Module>(filename, context);

        codegen::IRGenerator generator(context, std::move(module), errorHandler);

        // Generate LLVM IR from the AST
        auto generatedModule = generator.generate(program);
        if (errorHandler.hasFatalErrors() || !generatedModule)
        {
            return false;
        }

        // Verify the module
        std::string verifierErrors;
        llvm::raw_string_ostream verifierStream(verifierErrors);
        if (llvm::verifyModule(*generatedModule, &verifierStream))
        {
            errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                     "Invalid LLVM IR generated: " + verifierErrors,
                                     filename, 0, 0);
            return false;
        }

        // Optimize if requested
        if (options.optimize && !errorHandler.hasFatalErrors())
        {
            optimizeModule(*generatedModule, options.optimizationLevel);
        }

        // Dump IR if requested
        if (options.dumpIR)
        {
            std::string irOutput;
            llvm::raw_string_ostream irStream(irOutput);
            irStream << *generatedModule;
            std::cout << irOutput << std::endl;
        }

        // Write output if specified
        if (!options.outputFile.empty())
        {
            std::string outputPath = options.outputFile;
            if (outputPath.find('.') == std::string::npos)
            {
                outputPath += ".ll";
            }

            std::error_code EC;
            llvm::raw_fd_ostream outputFile(outputPath, EC);
            if (EC)
            {
                errorHandler.reportError(error::ErrorCode::I003_READ_ERROR,
                                         "Could not open output file: " + EC.message(),
                                         filename, 0, 0);
                return false;
            }

            outputFile << *generatedModule;
        }

        return !errorHandler.hasFatalErrors();
    }

private:
    error::ErrorHandler &errorHandler;
    type_checker::FeatureManager featureManager;

    void initializeFFI()
    {
        // FFI initialization is handled by the relevant modules if available
        // No global ffi::FFIInterface::initialize() needed here
    }

    void optimizeModule(llvm::Module &module, int level)
    {
        // Create a function pass manager
        llvm::PassBuilder passBuilder;
        llvm::LoopAnalysisManager LAM;
        llvm::FunctionAnalysisManager FAM;
        llvm::CGSCCAnalysisManager CGAM;
        llvm::ModuleAnalysisManager MAM;

        // Register all the basic analyses with the managers
        passBuilder.registerModuleAnalyses(MAM);
        passBuilder.registerCGSCCAnalyses(CGAM);
        passBuilder.registerFunctionAnalyses(FAM);
        passBuilder.registerLoopAnalyses(LAM);
        passBuilder.crossRegisterProxies(LAM, FAM, CGAM, MAM);

        // Create the optimization pipeline based on optimization level
        llvm::ModulePassManager MPM;
        if (level == 0)
        {
            // O0 - No optimization
            MPM = passBuilder.buildO0DefaultPipeline(llvm::OptimizationLevel::O0);
        }
        else if (level == 1)
        {
            // O1 - Basic optimizations
            MPM = passBuilder.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O1);
        }
        else if (level == 2)
        {
            // O2 - Default optimizations
            MPM = passBuilder.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O2);
        }
        else if (level == 3)
        {
            // O3 - Aggressive optimizations
            MPM = passBuilder.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O3);
        }

        // Run the optimizations
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
              << "  --no-ffi               Disable FFI support\n"
              << "  --no-concurrency       Disable concurrency features\n"
              << "  --no-advanced          Disable advanced language features\n"
              << "  --enable-python        Enable Python FFI (if available)\n"
              << "  --enable-javascript    Enable JavaScript FFI\n"
              << "  --enable-cpp           Enable C++ FFI\n"
              << std::endl;
    std::cout << "\nAdvanced Features:\n"
              << "  - Option/Result types for error handling\n"
              << "  - Traits and generics\n"
              << "  - Ownership and move semantics\n"
              << "  - Null safety\n"
              << "  - Concurrency with async/await\n"
              << "  - FFI support (Python, JavaScript, C++)\n"
              << "  - LINQ-style data processing\n"
              << "  - Extension functions\n"
              << std::endl;
}

/**
 * @brief Simple REPL for interactive compilation.
 */
void runRepl(Compiler &compiler, error::ErrorHandler &errorHandler)
{
    std::string line;
    Compiler::CompilationOptions options;
    options.dumpIR = true;
    options.enableFFI = true;
    options.enableConcurrency = true;
    options.enableAdvancedFeatures = true;
    options.optimize = true;
    options.optimizationLevel = 2;

    // Initialize REPL state
    static int replCounter = 0;
    std::string replState;

    std::cout << "Tocin REPL (type 'exit' to quit, 'clear' to reset)\n> ";

    while (std::getline(std::cin, line))
    {
        if (line == "exit" || line == "quit")
            break;
        if (line == "clear")
        {
            errorHandler.clearErrors();
            replState.clear();
            replCounter = 0;
            std::cout << "> ";
            continue;
        }

        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        
        if (line.empty())
        {
            std::cout << "> ";
            continue;
        }

        // Create a proper module for each REPL input
        std::string moduleSource;
        
        // Handle variable declarations and expressions
        if (line.find("let ") == 0 || line.find("const ") == 0)
        {
            // Variable declaration
            if (line.back() != ';')
                line += ";";
            moduleSource = replState + "\n" + line;
        }
        else if (line.find("def ") == 0 || line.find("class ") == 0 || 
                 line.find("trait ") == 0 || line.find("import ") == 0)
        {
            // Definition or import statement
            moduleSource = replState + "\n" + line;
        }
        else
        {
            // Expression - wrap in a main function if needed
            if (line.back() != ';')
                line += ";";
            
            // Create a temporary function to evaluate the expression
            std::string funcName = "repl_expr_" + std::to_string(replCounter++);
            moduleSource = replState + "\n" +
                         "def " + funcName + "() {\n" +
                         "    " + line + "\n" +
                         "}\n" +
                         funcName + "();";
        }

        // Compile the current state
        if (compiler.compile(moduleSource, "<repl>", options))
        {
            // Update REPL state with successful compilation
            replState = moduleSource;
        }
        else
        {
            // Clear errors but keep the previous state
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
        else if (arg == "--no-ffi")
        {
            options.enableFFI = false;
        }
        else if (arg == "--no-concurrency")
        {
            options.enableConcurrency = false;
        }
        else if (arg == "--no-advanced")
        {
            options.enableAdvancedFeatures = false;
        }
        else if (arg == "--enable-python")
        {
            options.enableFFI = true;
        }
        else if (arg == "--enable-javascript")
        {
            options.enableFFI = true;
        }
        else if (arg == "--enable-cpp")
        {
            options.enableFFI = true;
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
