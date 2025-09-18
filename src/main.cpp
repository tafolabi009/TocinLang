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

// New features
#include "compiler/macro_system.h"
#include "runtime/async_system.h"
#include "debugger/debugger.h"
#include "targets/wasm_target.h"
#include "package/package_manager.h"

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
 * @brief Enhanced compiler structure with all new features
 */
class EnhancedCompiler
{
public:
    EnhancedCompiler(error::ErrorHandler &errorHandler)
        : errorHandler(errorHandler), featureManager(errorHandler),
          macroSystem(std::make_unique<compiler::MacroSystem>()),
          asyncSystem(std::make_unique<runtime::AsyncSystem>()),
          debugger(std::make_unique<debugger::LLVMDebugger>()),
          wasmTarget(std::make_unique<targets::WASMTarget>()),
          packageManager(std::make_unique<package::PackageManager>(".", errorHandler))
    {
        // Initialize feature manager
        featureManager.initialize();
        
        // Initialize async system
        runtime::AsyncSystem::initialize();
        
        // Initialize debugger
        debugger->initialize();
        
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
        bool enableMacros;
        bool enableAsync;
        bool enableDebugger;
        bool enableWASM;
        std::string target;
        bool enablePackageManager;

        CompilationOptions()
            : dumpIR(false), optimize(false), optimizationLevel(2), outputFile(""),
              enableFFI(true), enableConcurrency(true), enableAdvancedFeatures(true),
              enableMacros(true), enableAsync(true), enableDebugger(false),
              enableWASM(false), target("native"), enablePackageManager(true) {}
    };

    bool compile(const std::string &source, const std::string &filename,
                 const CompilationOptions &options = CompilationOptions())
    {
        std::string processedSource = source;
        
        // Process macros if enabled
        if (options.enableMacros) {
            processedSource = processMacros(source, filename);
        }

        // Lexical analysis
        lexer::Lexer lexer(processedSource, filename, 4);
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

        // Type checking with advanced features
        type_checker::TypeChecker checker(errorHandler, compilationContext, &featureManager);
        checker.check(program);

        if (errorHandler.hasFatalErrors())
        {
            return false;
        }

        // Generate code based on target
        if (options.target == "wasm" && options.enableWASM) {
            return compileToWASM(program, filename, options);
        } else {
            return compileToNative(program, filename, options);
        }
    }

    bool compileToNative(ast::StmtPtr program, const std::string& filename, 
                        const CompilationOptions& options)
    {
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

    bool compileToWASM(ast::StmtPtr program, const std::string& filename,
                       const CompilationOptions& options)
    {
        targets::WASMTargetConfig config;
        config.optimize = options.optimize;
        config.enableSIMD = true;
        config.enableExceptionHandling = true;
        
        auto target = std::make_unique<targets::WASMTarget>(config);
        std::string wasmCode = target->generateWASM(program, errorHandler);
        
        if (errorHandler.hasFatalErrors() || wasmCode.empty())
        {
            return false;
        }

        // Optimize WASM if requested
        if (options.optimize) {
            wasmCode = target->optimizeWASM(wasmCode);
        }

        // Validate WASM
        if (!target->validateWASM(wasmCode, errorHandler)) {
            return false;
        }

        // Write WASM output
        if (!options.outputFile.empty()) {
            std::string outputPath = options.outputFile;
            if (outputPath.find('.') == std::string::npos) {
                outputPath += ".wasm";
            }

            std::ofstream outputFile(outputPath);
            if (!outputFile.is_open()) {
                errorHandler.reportError(error::ErrorCode::I003_READ_ERROR,
                                       "Could not open output file: " + outputPath,
                                       filename, 0, 0);
                return false;
            }

            outputFile << wasmCode;
        }

        return !errorHandler.hasFatalErrors();
    }

    // Package manager methods
    bool installPackage(const std::string& name, const std::string& version = "") {
        return packageManager->install(name, version);
    }

    bool uninstallPackage(const std::string& name) {
        return packageManager->uninstall(name);
    }

    std::vector<package::PackageInfo> searchPackages(const std::string& query) {
        return packageManager->search(query);
    }

    // Debugger methods
    void startDebugger() {
        if (debugger) {
            debugger->start();
        }
    }

    void setBreakpoint(const std::string& filename, int line, int column = 0) {
        if (debugger) {
            debugger->setBreakpoint(filename, line, column);
        }
    }

    void stepInto() {
        if (debugger) {
            debugger->stepInto();
        }
    }

    void stepOver() {
        if (debugger) {
            debugger->stepOver();
        }
    }

    void continueExecution() {
        if (debugger) {
            debugger->continueExecution();
        }
    }

    // Async methods
    template<typename T>
    runtime::Future<T> createAsync(std::function<T()> func) {
        return runtime::AsyncSystem::createAsync(func).execute();
    }

    template<typename T>
    T await(runtime::Future<T>& future) {
        return runtime::AsyncSystem::await(future);
    }

private:
    error::ErrorHandler &errorHandler;
    type_checker::FeatureManager featureManager;
    std::unique_ptr<compiler::MacroSystem> macroSystem;
    std::unique_ptr<runtime::AsyncSystem> asyncSystem;
    std::unique_ptr<debugger::Debugger> debugger;
    std::unique_ptr<targets::WASMTarget> wasmTarget;
    std::unique_ptr<package::PackageManager> packageManager;

    void initializeFFI()
    {
        // FFI initialization is handled by the relevant modules if available
    }

    std::string processMacros(const std::string& source, const std::string& filename) {
        // Process macros in the source code
        // This would expand macros before compilation
        return source; // Placeholder
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
              << "  --target <target>      Set compilation target (native, wasm)\n"
              << "  --no-ffi               Disable FFI support\n"
              << "  --no-concurrency       Disable concurrency features\n"
              << "  --no-advanced          Disable advanced language features\n"
              << "  --no-macros            Disable macro system\n"
              << "  --no-async             Disable async/await\n"
              << "  --debug                Enable debugger support\n"
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
              << "  - Macro system for compile-time code generation\n"
              << "  - FFI support (Python, JavaScript, C++)\n"
              << "  - LINQ-style data processing\n"
              << "  - Extension functions\n"
              << "  - WebAssembly target\n"
              << "  - Package manager\n"
              << "  - Debugger support\n"
              << std::endl;
}

/**
 * @brief Enhanced REPL with all new features
 */
void runEnhancedRepl(EnhancedCompiler &compiler, error::ErrorHandler &errorHandler)
{
    std::string line;
    EnhancedCompiler::CompilationOptions options;
    options.dumpIR = true;
    options.enableFFI = true;
    options.enableConcurrency = true;
    options.enableAdvancedFeatures = true;
    options.enableMacros = true;
    options.enableAsync = true;
    options.optimize = true;
    options.optimizationLevel = 2;

    // Initialize REPL state
    static int replCounter = 0;
    std::string replState;

    std::cout << "Tocin Enhanced REPL (type 'exit' to quit, 'clear' to reset)\n"
              << "Commands: debug, package, async, macro\n> ";

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

        // Handle special commands
        if (line == "debug") {
            std::cout << "Debugger commands: break, step, continue, variables, stack\n";
            std::cout << "> ";
            continue;
        }
        if (line == "package") {
            std::cout << "Package commands: install, uninstall, search, list\n";
            std::cout << "> ";
            continue;
        }
        if (line == "async") {
            std::cout << "Async commands: await, future, promise\n";
            std::cout << "> ";
            continue;
        }
        if (line == "macro") {
            std::cout << "Macro commands: define, expand, list\n";
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
 * @brief Main entry point for the enhanced Tocin compiler.
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

    // Create enhanced compiler
    EnhancedCompiler compiler(errorHandler);

    // If no arguments, run enhanced REPL
    if (argc == 1)
    {
        runEnhancedRepl(compiler, errorHandler);
        return 0;
    }

    // Parse command-line arguments
    EnhancedCompiler::CompilationOptions options;
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
        else if (arg == "--target" && i + 1 < argc)
        {
            options.target = argv[++i];
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
        else if (arg == "--no-macros")
        {
            options.enableMacros = false;
        }
        else if (arg == "--no-async")
        {
            options.enableAsync = false;
        }
        else if (arg == "--debug")
        {
            options.enableDebugger = true;
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
