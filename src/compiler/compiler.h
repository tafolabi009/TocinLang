#ifndef TOCIN_COMPILER_H
#define TOCIN_COMPILER_H

#include "../lexer/lexer.h"
#include "../parser/parser.h"
#include "../type/type_checker.h"
#include "../codegen/ir_generator.h"
#include "../error/error_handler.h"
#include "compilation_context.h"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <string>
#include <memory>

namespace compiler
{

    /**
     * @brief Compilation options for the Tocin compiler.
     */
    struct CompilationOptions
    {
        bool useCompression = false;
        bool serializeAst = false;
        bool optimize = true;
        bool dumpIR = false;
        int optimizationLevel = 2;
        std::string outputFile;
        bool generateObject = false;
        bool generateAssembly = false;
        bool runJIT = false;
    };

    /**
     * @brief Main compiler class for Tocin language.
     */
    class Compiler
    {
    public:
        /**
         * @brief Constructs a compiler.
         * @param errorHandler The error handler to use.
         */
        explicit Compiler(error::ErrorHandler &errorHandler);

        /**
         * @brief Compiles source code.
         * @param source The source code to compile.
         * @param filename The name of the source file.
         * @param options Compilation options.
         * @return True if compilation succeeds, false otherwise.
         */
        bool compile(const std::string &source, const std::string &filename,
                     const CompilationOptions &options = CompilationOptions());

        /**
         * @brief Gets the generated LLVM module.
         * @return The LLVM module.
         */
        std::unique_ptr<llvm::Module> getModule();

        /**
         * @brief Executes the compiled code using JIT.
         * @return The exit code of the program.
         */
        int executeJIT();

        /**
         * @brief Outputs the compiled code to the specified file.
         * @param outputFile The output file path.
         * @param generateObject Whether to generate an object file.
         * @param generateAssembly Whether to generate assembly.
         * @return True if output succeeds, false otherwise.
         */
        bool outputToFile(const std::string &outputFile, bool generateObject, bool generateAssembly);

    private:
        error::ErrorHandler &errorHandler;
        std::unique_ptr<llvm::LLVMContext> context;
        std::unique_ptr<llvm::Module> module;
        std::unique_ptr<llvm::ExecutionEngine> executionEngine;
        tocin::compiler::CompilationContext compilationContext;

        /**
         * @brief Initializes the LLVM targets.
         */
        void initializeLLVMTargets();

        /**
         * @brief Optimizes the generated LLVM module.
         * @param level The optimization level (0-3).
         * @return True if optimization succeeds, false otherwise.
         */
        bool optimizeModule(int level);

        /**
         * @brief Creates a target machine for the current platform.
         * @return The target machine or nullptr on failure.
         */
        std::unique_ptr<llvm::TargetMachine> createTargetMachine();
    };

} // namespace compiler

#endif // TOCIN_COMPILER_H
