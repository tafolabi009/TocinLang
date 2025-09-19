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

// Conditionally include Python
#ifdef WITH_PYTHON
#include <Python.h>
#endif

// Forward declarations
namespace error {
    enum class ErrorSeverity { INFO, WARNING, ERROR, FATAL };
    enum class ErrorCode { 
        I001_FILE_NOT_FOUND, 
        C001_UNIMPLEMENTED_FEATURE,
        C002_CODEGEN_ERROR,
        C003_TYPECHECK_ERROR,
        C004_INTERNAL_ASSERTION_FAILED
    };
    
    class ErrorHandler {
    public:
        ErrorHandler() = default;
        
        void reportError(ErrorCode code, const std::string& message, 
                         const std::string& filename = "", int line = 0, int column = 0,
                         ErrorSeverity severity = ErrorSeverity::ERROR) {
            std::cerr << getErrorPrefix(severity) << " " << message << std::endl;
            
            if (!filename.empty()) {
                std::cerr << "  at " << filename;
                if (line > 0) {
                    std::cerr << ":" << line;
                    if (column > 0) {
                        std::cerr << ":" << column;
                    }
                }
                std::cerr << std::endl;
            }
            
            if (severity == ErrorSeverity::FATAL) {
                hasFatalErrors_ = true;
            } else if (severity == ErrorSeverity::ERROR) {
                hasErrors_ = true;
            }
        }
        
        bool hasErrors() const { return hasErrors_ || hasFatalErrors_; }
        bool hasFatalErrors() const { return hasFatalErrors_; }
        void clearErrors() { hasErrors_ = false; hasFatalErrors_ = false; }
        
    private:
        bool hasErrors_ = false;
        bool hasFatalErrors_ = false;
        
        std::string getErrorPrefix(ErrorSeverity severity) {
            switch (severity) {
                case ErrorSeverity::INFO: return "[INFO]";
                case ErrorSeverity::WARNING: return "[WARNING]";
                case ErrorSeverity::ERROR: return "[ERROR]";
                case ErrorSeverity::FATAL: return "[FATAL]";
                default: return "[UNKNOWN]";
            }
        }
    };
}

/**
 * @brief Simple compiler structure to hold compilation options.
 */
class Compiler {
public:
    Compiler(error::ErrorHandler& errorHandler) : errorHandler(errorHandler) {}
    
    struct CompilationOptions {
        bool dumpIR = false;
        bool optimize = false;
        int optimizationLevel = 2;
        std::string outputFile;
    };
    
    bool compile(const std::string& source, const std::string& filename, 
                 const CompilationOptions& options = CompilationOptions()) {
        std::cout << "Compiling " << filename << "..." << std::endl;
        
        // Create LLVM context and module
        llvm::LLVMContext context;
        auto module = std::make_unique<llvm::Module>(filename, context);
        
        // Create a simple main function
        llvm::FunctionType *mainType = llvm::FunctionType::get(
            llvm::Type::getInt32Ty(context), false);
        llvm::Function *mainFunction = llvm::Function::Create(
            mainType, llvm::Function::ExternalLinkage, "main", module.get());
        
        // Create basic block for the main function
        llvm::BasicBlock *block = llvm::BasicBlock::Create(context, "entry", mainFunction);
        llvm::IRBuilder<> builder(context);
        builder.SetInsertPoint(block);
        
        // For now, our compiler just outputs the source length
        builder.CreateCall(
            createPrintfFunction(*module),
            {builder.CreateGlobalStringPtr("Source length: %d characters\n"),
             llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), source.length())}
        );
        
        // Return 0 from main
        builder.CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0));
        
        // Verify the module
        std::string error;
        llvm::raw_string_ostream errorStream(error);
        if (llvm::verifyModule(*module, &errorStream)) {
            errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                    "Module verification failed: " + errorStream.str());
            return false;
        }
        
        // Print the IR if requested
        if (options.dumpIR) {
            std::string ir;
            llvm::raw_string_ostream irStream(ir);
            module->print(irStream, nullptr);
            std::cout << "\nGenerated LLVM IR:\n" << irStream.str() << std::endl;
        }
        
        // Write IR to file if output file is specified
        if (!options.outputFile.empty()) {
            std::error_code EC;
            llvm::raw_fd_ostream outFile(options.outputFile, EC, llvm::sys::fs::OF_None);
            if (EC) {
                errorHandler.reportError(error::ErrorCode::I001_FILE_NOT_FOUND,
                                        "Could not open output file: " + options.outputFile);
                return false;
            }
            module->print(outFile, nullptr);
            std::cout << "IR written to " << options.outputFile << std::endl;
        }
        
        std::cout << "Compilation successful!" << std::endl;
        return true;
    }
    
private:
    error::ErrorHandler& errorHandler;
    
    // Create external printf declaration
    llvm::Function* createPrintfFunction(llvm::Module& module) {
        llvm::LLVMContext& context = module.getContext();
        llvm::FunctionType* printfType = llvm::FunctionType::get(
            llvm::Type::getInt32Ty(context),
            {llvm::Type::getInt8PtrTy(context)},
            true);
            
        return llvm::Function::Create(
            printfType, llvm::Function::ExternalLinkage, "printf", &module);
    }
};

/**
 * @brief Displays usage information.
 */
void displayUsage() {
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
void runRepl(Compiler& compiler, error::ErrorHandler& errorHandler) {
    std::string line;
    std::stringstream source;
    Compiler::CompilationOptions options;
    options.dumpIR = true;
    
    std::cout << "Tocin REPL (type 'exit' to quit, 'clear' to reset)\n> ";
    
    while (std::getline(std::cin, line)) {
        if (line == "exit")
            break;
        if (line == "clear") {
            source.str("");
            errorHandler.clearErrors();
            std::cout << "> ";
            continue;
        }
        
        source << line << "\n";
        if (compiler.compile(source.str(), "<repl>", options)) {
            // Success
        } else {
            // Clear errors to continue the REPL
            errorHandler.clearErrors();
        }
        
        std::cout << "> ";
    }
}

/**
 * @brief Main entry point for the Tocin compiler.
 */
int main(int argc, char* argv[]) {
    // Initialize LLVM
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    
    // Initialize Python if available
    #ifdef WITH_PYTHON
    Py_Initialize();
    #endif
    
    // Create error handler
    error::ErrorHandler errorHandler;
    
    // Create compiler
    Compiler compiler(errorHandler);
    
    // If no arguments, run REPL
    if (argc == 1) {
        runRepl(compiler, errorHandler);
        return 0;
    }
    
    // Parse command-line arguments
    Compiler::CompilationOptions options;
    std::string filename;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help") {
            displayUsage();
            return 0;
        }
        else if (arg == "--dump-ir") {
            options.dumpIR = true;
        }
        else if (arg == "-O0") {
            options.optimize = true;
            options.optimizationLevel = 0;
        }
        else if (arg == "-O1") {
            options.optimize = true;
            options.optimizationLevel = 1;
        }
        else if (arg == "-O2") {
            options.optimize = true;
            options.optimizationLevel = 2;
        }
        else if (arg == "-O3") {
            options.optimize = true;
            options.optimizationLevel = 3;
        }
        else if (arg == "-o" && i + 1 < argc) {
            options.outputFile = argv[++i];
        }
        else if (arg[0] == '-') {
            std::cerr << "Unknown option: " << arg << std::endl;
            displayUsage();
            return 1;
        }
        else {
            filename = arg;
        }
    }
    
    // Check if a filename was provided
    if (filename.empty()) {
        std::cerr << "Error: No input file specified.\n";
        displayUsage();
        return 1;
    }
    
    // Read the source file
    std::ifstream file(filename);
    if (!file.is_open()) {
        errorHandler.reportError(error::ErrorCode::I001_FILE_NOT_FOUND,
                                "Could not open file: " + filename);
        return 1;
    }
    
    std::string source((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
    file.close();
    
    // Compile the source
    if (!compiler.compile(source, filename, options)) {
        return 1;
    }
    
    // Clean up Python if it was initialized
    #ifdef WITH_PYTHON
    Py_Finalize();
    #endif
    
    return 0;
}
