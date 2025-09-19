#include "lexer/lexer.h"
#include "parser/parser.h"
#include "type/type_checker.h"
#include "codegen/ir_generator.h"
#include "compiler/compiler.h"
#include "error/error_handler.h"
#include "ffi/ffi_python.h"
#include "ffi/ffi_javascript.h"

#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/Support/TargetSelect.h>
#include <v8.h>
#include <Python.h>
#include <zlib.h>
#include <zstd.h>
#include <libxml/parser.h>
#include <libffi.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <memory>

namespace tocin {

    /**
     * @brief Initializes the V8 JavaScript engine.
     * @return A unique pointer to the V8 isolate.
     */
    std::unique_ptr<v8::Isolate, void(*)(v8::Isolate*)> initializeV8() {
        v8::V8::InitializeICU();
        v8::V8::InitializeExternalStartupData("");
        auto platform = v8::platform::NewDefaultPlatform();
        v8::V8::InitializePlatform(platform.get());
        v8::V8::Initialize();
        v8::Isolate::CreateParams create_params;
        create_params.array_buffer_allocator =
            v8::ArrayBuffer::Allocator::NewDefaultAllocator();
        auto isolate = v8::Isolate::New(create_params);
        return { isolate, [](v8::Isolate* i) { i->Dispose(); } };
    }

    /**
     * @brief Executes JavaScript code via V8 for FFI.
     * @param isolate The V8 isolate.
     * @param code The JavaScript code to execute.
     * @param errorHandler The error handler for reporting issues.
     * @return True if execution succeeds, false otherwise.
     */
    bool executeJavaScript(v8::Isolate* isolate, const std::string& code,
        error::ErrorHandler& errorHandler) {
        v8::Isolate::Scope isolate_scope(isolate);
        v8::HandleScope handle_scope(isolate);
        v8::Local<v8::Context> context = v8::Context::New(isolate);
        v8::Context::Scope context_scope(context);

        v8::Local<v8::String> source =
            v8::String::NewFromUtf8(isolate, code.c_str()).ToLocalChecked();
        v8::Local<v8::Script> script;
        if (!v8::Script::Compile(context, source).ToLocal(&script)) {
            errorHandler.reportError("Failed to compile JavaScript code", "", 0, 0,
                error::ErrorSeverity::ERROR);
            return false;
        }

        v8::Local<v8::Value> result;
        if (!script->Run(context).ToLocal(&result)) {
            errorHandler.reportError("Failed to execute JavaScript code", "", 0, 0,
                error::ErrorSeverity::ERROR);
            return false;
        }
        return true;
    }

    /**
     * @brief Initializes the Python interpreter.
     */
    void initializePython() {
        Py_Initialize();
    }

    /**
     * @brief Executes Python code for FFI.
     * @param code The Python code to execute.
     * @param errorHandler The error handler for reporting issues.
     * @return True if execution succeeds, false otherwise.
     */
    bool executePython(const std::string& code, error::ErrorHandler& errorHandler) {
        if (PyRun_SimpleString(code.c_str()) != 0) {
            errorHandler.reportError("Failed to execute Python code", "", 0, 0,
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
    std::string compressSourceZlib(const std::string& source, error::ErrorHandler& errorHandler) {
        z_stream stream = { 0 };
        if (deflateInit(&stream, Z_BEST_COMPRESSION) != Z_OK) {
            errorHandler.reportError("Failed to initialize ZLIB compression", "", 0, 0,
                error::ErrorSeverity::ERROR);
            return "";
        }

        stream.next_in = (Bytef*)source.c_str();
        stream.avail_in = source.size();
        std::string compressed;
        std::vector<char> buffer(1024);

        do {
            stream.next_out = (Bytef*)buffer.data();
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
    std::string compressSourceZstd(const std::string& source, error::ErrorHandler& errorHandler) {
        size_t const cBuffSize = ZSTD_compressBound(source.size());
        std::vector<char> cBuff(cBuffSize);
        size_t const cSize = ZSTD_compress(cBuff.data(), cBuffSize, source.c_str(),
            source.size(), ZSTD_maxCLevel());
        if (ZSTD_isError(cSize)) {
            errorHandler.reportError("Failed to compress with zstd: " + std::string(ZSTD_getErrorName(cSize)),
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
    std::string serializeAstToXml(ast::StmtPtr ast, error::ErrorHandler& errorHandler) {
        xmlDocPtr doc = xmlNewDoc(BAD_CAST "1.0");
        xmlNodePtr root = xmlNewNode(nullptr, BAD_CAST "AST");
        xmlDocSetRootElement(doc, root);

        // Simplified: Add a placeholder node
        xmlNewChild(root, nullptr, BAD_CAST "Statement", BAD_CAST "SerializedAST");

        xmlChar* xmlbuff;
        int buffersize;
        xmlDocDumpFormatMemory(doc, &xmlbuff, &buffersize, 1);
        std::string result = reinterpret_cast<char*>(xmlbuff);
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
    bool executeModule(std::unique_ptr<llvm::Module> module, error::ErrorHandler& errorHandler) {
        llvm::InitializeNativeTarget();
        llvm::InitializeNativeTargetAsmPrinter();
        llvm::InitializeNativeTargetAsmParser();

        std::string errStr;
        auto engine = llvm::EngineBuilder(std::move(module))
            .setErrorStr(&errStr)
            .setMCJITMemoryManager(std::make_unique<llvm::SectionMemoryManager>())
            .create();
        if (!engine) {
            errorHandler.reportError("Failed to create execution engine: " + errStr, "", 0, 0,
                error::ErrorSeverity::ERROR);
            return false;
        }

        auto mainFunc = engine->FindFunctionNamed("main");
        if (!mainFunc) {
            errorHandler.reportError("No main function found", "", 0, 0, error::ErrorSeverity::ERROR);
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
    bool compileAndExecute(const std::string& source, const std::string& filename,
        error::ErrorHandler& errorHandler, v8::Isolate* v8Isolate,
        bool useCompression = false, bool serializeAst = false) {
        std::string processedSource = source;

        // Compress source if requested
        if (useCompression) {
            processedSource = compressSourceZstd(source, errorHandler);
            if (processedSource.empty()) {
                processedSource = compressSourceZlib(source, errorHandler);
                if (processedSource.empty()) return false;
            }
        }

        // Lexing
        lexer::Lexer lexer(processedSource, filename);
        auto tokens = lexer.scanTokens();
        if (errorHandler.hasErrors()) return false;

        // Parsing
        parser::Parser parser(tokens);
        auto ast = parser.parse();
        if (!ast || errorHandler.hasErrors()) return false;

        // Serialize AST to XML if requested
        if (serializeAst) {
            auto xml = serializeAstToXml(ast, errorHandler);
            if (!xml.empty()) {
                std::cout << "Serialized AST:\n" << xml << std::endl;
            }
        }

        // Type Checking
        type_checker::TypeChecker typeChecker(errorHandler);
        typeChecker.check(ast);
        if (errorHandler.hasErrors()) return false;

        // IR Generation
        llvm::LLVMContext context;
        auto module = std::make_unique<llvm::Module>("tocin_module", context);
        ir_generator::IRGenerator irGen(context, std::move(module), errorHandler);
        module = irGen.generate(ast);
        if (!module || errorHandler.hasErrors()) return false;

        // Execute JavaScript FFI example (simplified)
        if (v8Isolate) {
            std::string jsCode = "function example() { return 'Hello from V8'; }; example();";
            if (!executeJavaScript(v8Isolate, jsCode, errorHandler)) {
                return false;
            }
        }

        // Execute Python FFI example (simplified)
        std::string pyCode = "print('Hello from Python')";
        if (!executePython(pyCode, errorHandler)) {
            return false;
        }

        // Execute LLVM module
        return executeModule(std::move(module), errorHandler);
    }

    /**
     * @brief Runs the REPL mode for interactive compilation.
     * @param compiler The compiler to use.
     */
    void runRepl(compiler::Compiler& compiler, error::ErrorHandler& errorHandler) {
        std::string line;
        std::stringstream source;
        compiler::CompilationOptions options;
        options.runJIT = true;
        
        std::cout << "Tocin REPL (type 'exit' to quit, 'clear' to reset)\n> ";
        
        while (std::getline(std::cin, line)) {
            if (line == "exit") break;
            if (line == "clear") {
                source.str("");
                errorHandler.clearErrors();
                std::cout << "> ";
                continue;
            }
            
            source << line << "\n";
            if (compiler.compile(source.str(), "<repl>", options)) {
                int result = compiler.executeJIT();
                if (result != 0) {
                    std::cout << "Program exited with code: " << result << std::endl;
                }
            } else {
                // Clear errors to continue the REPL
                errorHandler.clearErrors();
            }
            
            std::cout << "> ";
        }
    }

    /**
     * @brief Displays usage information.
     */
    void displayUsage() {
        std::cout << "Usage: tocin [options] [filename]\n"
                  << "Options:\n"
                  << "  --help                 Display this help message\n"
                  << "  --compress             Compress source code before compilation\n"
                  << "  --serialize-ast        Serialize AST to XML\n"
                  << "  --dump-ir              Dump LLVM IR to stdout\n"
     * @brief Main entry point for the Tocin compiler.
     * @param argc Number of command-line arguments.
     * @param argv Array of command-line arguments.
     * @return Exit code (0 for success, 1 for failure).
     */
    int main(int argc, char* argv[]) {
        error::ErrorHandler errorHandler;

        // Initialize V8
        auto v8Isolate = initializeV8();
        if (!v8Isolate) {
            errorHandler.reportError("Failed to initialize V8", "", 0, 0,
                error::ErrorSeverity::ERROR);
            return 1;
        }

        // Initialize Python
        initializePython();

        // Handle command-line arguments
        bool useCompression = false;
        bool serializeAst = false;
        std::string filename;

        if (argc == 1) {
            runRepl(errorHandler, v8Isolate.get());
            Py_Finalize();
            return 0;
        }

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--compress") {
                useCompression = true;
            }
            else if (arg == "--serialize-ast") {
                serializeAst = true;
            }
            else {
                filename = arg;
            }
        }

        if (filename.empty()) {
            errorHandler.reportError("Usage: tocin [filename] [--compress] [--serialize-ast]", "", 0, 0,
                error::ErrorSeverity::ERROR);
            Py_Finalize();
            return 1;
        }

        std::ifstream file(filename);
        if (!file.is_open()) {
            errorHandler.reportError("Could not open file: " + filename, "", 0, 0,
                error::ErrorSeverity::ERROR);
            Py_Finalize();
            return 1;
        }

        std::string source((std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>());
        file.close();

        if (!compileAndExecute(source, filename, errorHandler, v8Isolate.get(),
            useCompression, serializeAst)) {
            Py_Finalize();
            return 1;
        }

        Py_Finalize();
        return 0;
    }

} // namespace tocin
