#include "lexer/lexer.h"
#include "parser/parser.h"
#include "type/type_checker.h"
#include "codegen/ir_generator.h"
#include "compiler/compiler.h"
#include "error/error_handler.h"

// System headers first
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <memory>

// LLVM headers
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/Support/TargetSelect.h>

// V8 headers with guards
#ifndef V8_HEADERS_INCLUDED
#define V8_HEADERS_INCLUDED
#include <v8.h>
#include <v8-platform.h>
#include <libplatform/libplatform.h>
#endif

// Python headers with guards
#ifndef PYTHON_HEADERS_INCLUDED
#define PYTHON_HEADERS_INCLUDED
#include <Python.h>
#endif

// Other system headers
#include <zlib.h>
#include <zstd.h>
#include <libxml/parser.h>
#include <ffi.h>

// FFI headers last
#include "ffi/ffi_python.h"
#include "ffi/ffi_javascript.h"

namespace tocin
{

    /**
     * @brief Initializes the V8 JavaScript engine.
     * @return A unique pointer to the V8 isolate.
     */
    std::unique_ptr<v8::Isolate, void (*)(v8::Isolate *)> initializeV8()
    {
        v8::V8::InitializeICU();
        v8::V8::InitializeExternalStartupData("");
        auto platform = v8::platform::NewDefaultPlatform();
        v8::V8::InitializePlatform(platform.get());
        v8::V8::Initialize();
        v8::Isolate::CreateParams create_params;
        create_params.array_buffer_allocator =
            v8::ArrayBuffer::Allocator::NewDefaultAllocator();
        auto isolate = v8::Isolate::New(create_params);
        return {isolate, [](v8::Isolate *i)
                { i->Dispose(); }};
    }

    /**
     * @brief Executes JavaScript code via V8 for FFI.
     * @param isolate The V8 isolate.
     * @param code The JavaScript code to execute.
     * @param errorHandler The error handler for reporting issues.
     * @return True if execution succeeds, false otherwise.
     */
    bool executeJavaScript(v8::Isolate *isolate, const std::string &code,
                           error::ErrorHandler &errorHandler)
    {
        v8::Isolate::Scope isolate_scope(isolate);
        v8::HandleScope handle_scope(isolate);
        v8::Local<v8::Context> context = v8::Context::New(isolate);
        v8::Context::Scope context_scope(context);

        v8::Local<v8::String> source =
            v8::String::NewFromUtf8(isolate, code.c_str()).ToLocalChecked();
        v8::Local<v8::Script> script;
        if (!v8::Script::Compile(context, source).ToLocal(&script))
        {
            errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                     "Failed to compile JavaScript code", "", 0, 0,
                                     error::ErrorSeverity::ERROR);
            return false;
        }

        v8::Local<v8::Value> result;
        if (!script->Run(context).ToLocal(&result))
        {
            errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                     "Failed to execute JavaScript code", "", 0, 0,
                                     error::ErrorSeverity::ERROR);
            return false;
        }
        return true;
    }

    /**
     * @brief Initializes the Python interpreter.
     */
    void initializePython()
    {
        Py_Initialize();
    }

    /**
     * @brief Executes Python code for FFI.
     * @param code The Python code to execute.
     * @param errorHandler The error handler for reporting issues.
     * @return True if execution succeeds, false otherwise.
     */
    bool executePython(const std::string &code, error::ErrorHandler &errorHandler)
    {
        if (PyRun_SimpleString(code.c_str()) != 0)
        {
            errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                     "Failed to execute Python code", "", 0, 0,
                                     error::ErrorSeverity::ERROR);
            return false;
        }
        return true;
    }

    /**
     * @brief Compresses source code using ZLIB.
     * @param source The source code to compress.
     * @param errorHandler The error handler for reporting issues.
     * @return The compressed data or empty string on failure.
     */
    std::string compressSourceZlib(const std::string &source, error::ErrorHandler &errorHandler)
    {
        z_stream stream = {0};
        if (deflateInit(&stream, Z_BEST_COMPRESSION) != Z_OK)
        {
            errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                     "Failed to initialize ZLIB compression", "", 0, 0,
                                     error::ErrorSeverity::ERROR);
            return "";
        }

        stream.next_in = (Bytef *)source.c_str();
        stream.avail_in = source.size();
        std::string compressed;
        std::vector<char> buffer(1024);

        do
        {
            stream.next_out = (Bytef *)buffer.data();
            stream.avail_out = buffer.size();
            deflate(&stream, Z_FINISH);
            compressed.append(buffer.data(), buffer.size() - stream.avail_out);
        } while (stream.avail_out == 0);

        deflateEnd(&stream);
        return compressed;
    }

    /**
     * @brief Compresses source code using zstd.
     * @param source The source code to compress.
     * @param errorHandler The error handler for reporting issues.
     * @return The compressed data or empty string on failure.
     */
    std::string compressSourceZstd(const std::string &source, error::ErrorHandler &errorHandler)
    {
        size_t const cBuffSize = ZSTD_compressBound(source.size());
        std::vector<char> cBuff(cBuffSize);
        size_t const cSize = ZSTD_compress(cBuff.data(), cBuffSize, source.c_str(),
                                           source.size(), ZSTD_maxCLevel());
        if (ZSTD_isError(cSize))
        {
            errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                     "Failed to compress with zstd: " + std::string(ZSTD_getErrorName(cSize)),
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            return "";
        }
        return std::string(cBuff.data(), cSize);
    }

    /**
     * @brief Serializes AST to XML using LibXml2.
     * @param ast The AST to serialize.
     * @param errorHandler The error handler for reporting issues.
     * @return The XML string or empty string on failure.
     */
    std::string serializeAstToXml(ast::StmtPtr ast, error::ErrorHandler &errorHandler)
    {
        xmlDocPtr doc = xmlNewDoc(BAD_CAST "1.0");
        xmlNodePtr root = xmlNewNode(nullptr, BAD_CAST "AST");
        xmlDocSetRootElement(doc, root);

        // Simplified: Add a placeholder node
        xmlNewChild(root, nullptr, BAD_CAST "Statement", BAD_CAST "SerializedAST");

        xmlChar *xmlbuff;
        int buffersize;
        xmlDocDumpFormatMemory(doc, &xmlbuff, &buffersize, 1);
        std::string result = reinterpret_cast<char *>(xmlbuff);
        xmlFree(xmlbuff);
        xmlFreeDoc(doc);
        return result;
    }

    /**
     * @brief Executes the given LLVM module using MCJIT.
     * @param module The LLVM module to execute.
     * @param errorHandler The error handler for reporting issues.
     * @return True if execution succeeds, false otherwise.
     */
    bool executeModule(std::unique_ptr<llvm::Module> module, error::ErrorHandler &errorHandler)
    {
        llvm::InitializeNativeTarget();
        llvm::InitializeNativeTargetAsmPrinter();
        llvm::InitializeNativeTargetAsmParser();

        std::string errStr;
        auto engine = llvm::EngineBuilder(std::move(module))
                          .setErrorStr(&errStr)
                          .setMCJITMemoryManager(std::make_unique<llvm::SectionMemoryManager>())
                          .create();
        if (!engine)
        {
            errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                     "Failed to create execution engine: " + errStr, "", 0, 0,
                                     error::ErrorSeverity::ERROR);
            return false;
        }

        auto mainFunc = engine->FindFunctionNamed("main");
        if (!mainFunc)
        {
            errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                     "No main function found", "", 0, 0, error::ErrorSeverity::ERROR);
            return false;
        }

        engine->finalizeObject();
        typedef void (*MainFunc)();
        auto funcPtr = (MainFunc)engine->getPointerToFunction(mainFunc);
        funcPtr();
        return true;
    }

    /**
     * @brief Compiles and executes the given source code.
     * @param source The source code to compile.
     * @param filename The name of the source file.
     * @param errorHandler The error handler for reporting issues.
     * @param v8Isolate The V8 isolate for JavaScript FFI.
     * @param useCompression Whether to compress the source code.
     * @param serializeAst Whether to serialize the AST to XML.
     * @return True if compilation and execution succeed, false otherwise.
     */
    bool compileAndExecute(const std::string &source, const std::string &filename,
                           error::ErrorHandler &errorHandler, v8::Isolate *v8Isolate,
                           bool useCompression = false, bool serializeAst = false)
    {
        std::string processedSource = source;

        // Compress source if requested
        if (useCompression)
        {
            processedSource = compressSourceZstd(source, errorHandler);
            if (processedSource.empty())
            {
                processedSource = compressSourceZlib(source, errorHandler);
                if (processedSource.empty())
                    return false;
            }
        }

        // Lexing
        lexer::Lexer lexer(processedSource, filename);
        auto tokens = lexer.tokenize();
        if (errorHandler.hasErrors())
            return false;

        // Parsing
        parser::Parser parser(tokens);
        auto ast = parser.parse();
        if (!ast || errorHandler.hasErrors())
            return false;

        // Serialize AST to XML if requested
        if (serializeAst)
        {
            auto xml = serializeAstToXml(ast, errorHandler);
            if (!xml.empty())
            {
                std::cout << "Serialized AST:\n"
                          << xml << std::endl;
            }
        }

        // Type Checking
        type_checker::TypeChecker typeChecker(errorHandler);
        typeChecker.check(ast);
        if (errorHandler.hasErrors())
            return false;

        // IR Generation
        llvm::LLVMContext context;
        auto module = std::make_unique<llvm::Module>("tocin_module", context);
        ir_generator::IRGenerator irGen(context, std::move(module), errorHandler);
        module = irGen.generate(ast);
        if (!module || errorHandler.hasErrors())
            return false;

        // Execute JavaScript FFI example (simplified)
        if (v8Isolate)
        {
            std::string jsCode = "function example() { return 'Hello from V8'; }; example();";
            if (!executeJavaScript(v8Isolate, jsCode, errorHandler))
            {
                return false;
            }
        }

        // Execute Python FFI example (simplified)
        std::string pyCode = "print('Hello from Python')";
        if (!executePython(pyCode, errorHandler))
        {
            return false;
        }

        // Execute LLVM module
        return executeModule(std::move(module), errorHandler);
    }

    /**
     * @brief Runs the REPL mode for interactive compilation.
     * @param compiler The compiler to use.
     */
    void runRepl(compiler::Compiler &compiler, error::ErrorHandler &errorHandler)
    {
        std::string line;
        std::stringstream source;
        compiler::CompilationOptions options;
        options.runJIT = true;

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
                int result = compiler.executeJIT();
                if (result != 0)
                {
                    std::cout << "Program exited with code: " << result << std::endl;
                }
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
     * @brief Displays usage information.
     */
    void displayUsage()
    {
        std::cout << "Usage: tocin [options] [filename]\n"
                  << "Options:\n"
                  << "  --help                 Display this help message\n"
                  << "  --compress             Compress source code before compilation\n"
                  << "  --serialize-ast        Serialize AST to XML\n"
                  << "  --dump-ir              Dump LLVM IR to stdout\n"
                  << "  -O0, -O1, -O2, -O3     Set optimization level (default: -O2)\n"
                  << "  -o <file>              Write output to <file>\n"
                  << "  -c                     Generate object file\n"
                  << "  -S                     Generate assembly file\n"
                  << "  --jit                  Run the program using JIT compilation\n"
                  << std::endl;
    }

    /**
     * @brief Main entry point for the Tocin compiler.
     * @param argc Number of command-line arguments.
     * @param argv Array of command-line arguments.
     * @return Exit code (0 for success, 1 for failure).
     */
    int main(int argc, char *argv[])
    {
        // Initialize error handler
        error::ErrorHandler errorHandler;

        // Create the compiler
        compiler::Compiler compiler(errorHandler);

        // Initialize the FFI systems
        ffi::PythonFFI pythonFFI;
        ffi::JavaScriptFFI jsFFI;

        // If no arguments, run REPL
        if (argc == 1)
        {
            runRepl(compiler, errorHandler);
            return 0;
        }

        // Parse command-line arguments
        compiler::CompilationOptions options;
        std::string filename;

        for (int i = 1; i < argc; ++i)
        {
            std::string arg = argv[i];

            if (arg == "--help")
            {
                displayUsage();
                return 0;
            }
            else if (arg == "--compress")
            {
                options.useCompression = true;
            }
            else if (arg == "--serialize-ast")
            {
                options.serializeAst = true;
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
            else if (arg == "-c")
            {
                options.generateObject = true;
            }
            else if (arg == "-S")
            {
                options.generateAssembly = true;
            }
            else if (arg == "--jit")
            {
                options.runJIT = true;
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
                                     "Could not open file: " + filename,
                                     "", 0, 0, error::ErrorSeverity::ERROR);
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

        // Execute the compiled code if requested
        if (options.runJIT)
        {
            return compiler.executeJIT();
        }

        return 0;
    }

} // namespace tocin
