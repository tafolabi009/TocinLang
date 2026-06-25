#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <memory>
#include <vector>
#include <set>
#include <functional>
#include <filesystem>
#include <iterator>

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
// JIT execution (ORCv2) and native object emission
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/ExecutionEngine/Orc/ExecutionUtils.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Support/CodeGen.h>
#include <cstdlib>
#include <cstdint>

// The Tocin runtime (channels/goroutines) lives in the static core library.
// Declared here so the JIT can register their addresses; referencing them in
// runJIT also forces the runtime objects to be linked into the compiler.
extern "C" {
    void *__tocin_chan_new();
    void __tocin_chan_send(void *, int64_t);
    int64_t __tocin_chan_recv(void *);
    void __tocin_go(void (*)(void *), void *);
    void __tocin_join_all();
    int8_t __tocin_chan_try_recv(void *, int64_t *);
    void __tocin_chan_park();
    void __tocin_try_register(void *);
    void __tocin_try_pop();
    int64_t __tocin_exc_value();
    void __tocin_throw(int64_t);
    // Dynamic collections
    void *__tocin_vec_new();
    void __tocin_vec_push(void *, int64_t);
    int64_t __tocin_vec_get(void *, int64_t);
    void __tocin_vec_set(void *, int64_t, int64_t);
    int64_t __tocin_vec_len(void *);
    int64_t __tocin_vec_pop(void *);
    void __tocin_vec_free(void *);
    void *__tocin_map_new();
    void __tocin_map_put(void *, int64_t, int64_t);
    int64_t __tocin_map_get(void *, int64_t);
    int64_t __tocin_map_has(void *, int64_t);
    void __tocin_map_put_str(void *, const char *, int64_t);
    int64_t __tocin_map_get_str(void *, const char *);
    int64_t __tocin_map_has_str(void *, const char *);
    int64_t __tocin_map_len(void *);
    void __tocin_map_free(void *);
    // Strings
    int64_t __tocin_str_len(const char *);
    int64_t __tocin_str_char_at(const char *, int64_t);
    char *__tocin_str_substring(const char *, int64_t, int64_t);
    int64_t __tocin_str_eq(const char *, const char *);
    int64_t __tocin_str_cmp(const char *, const char *);
    int64_t __tocin_str_index_of_char(const char *, int64_t);
    char *__tocin_int_to_str(int64_t);
    int64_t __tocin_str_to_int(const char *);
    char *__tocin_char_to_str(int64_t);
    char *__tocin_str_concat(const char *, const char *);
    char *__tocin_str_to_upper(const char *);
    char *__tocin_str_to_lower(const char *);
    int64_t __tocin_str_index_of(const char *, const char *);
    int64_t __tocin_str_contains(const char *, const char *);
    int64_t __tocin_str_starts_with(const char *, const char *);
    int64_t __tocin_str_ends_with(const char *, const char *);
    void __tocin_free(void *);
    void *__tocin_alloc(int64_t);
    void *__tocin_realloc(void *, int64_t);
    double __tocin_str_to_float(const char *);
    char *__tocin_float_to_str(double);
    // File I/O
    char *__tocin_read_file(const char *);
    int64_t __tocin_write_file(const char *, const char *);
    int64_t __tocin_append_file(const char *, const char *);
    char *__tocin_read_line();
}

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
#ifdef WITH_DEBUGGER
#include "debugger/debugger.h"
#endif
#ifdef WITH_WASM
#include "targets/wasm_target.h"
#endif
#ifdef WITH_PACKAGE_MANAGER
#include "package/package_manager.h"
#endif

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
          asyncSystem(std::make_unique<runtime::AsyncSystem>())
#ifdef WITH_DEBUGGER
          ,debugger(std::make_unique<debugger::LLVMDebugger>())
#endif
#ifdef WITH_WASM
          ,wasmTarget(std::make_unique<targets::WASMTarget>())
#endif
#ifdef WITH_PACKAGE_MANAGER
          ,packageManager(std::make_unique<package::PackageManager>(".", errorHandler))
#endif
    {
        // Initialize feature manager
        featureManager.initialize();
        
        // Initialize async system
        runtime::AsyncSystem::initialize();
        
#ifdef WITH_DEBUGGER
        if (debugger) debugger->initialize();
#endif
        
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
        bool run; // JIT-execute the program in-process (--jit / --run)

        CompilationOptions()
            : dumpIR(false), optimize(false), optimizationLevel(2), outputFile(""),
              enableFFI(true), enableConcurrency(true), enableAdvancedFeatures(true),
              enableMacros(true), enableAsync(true), enableDebugger(false),
              enableWASM(false), target("native"), enablePackageManager(true), run(false) {}
    };

    // Exit code produced by the most recent JIT execution (--run).
    int programExitCode = 0;
    int getProgramExitCode() const { return programExitCode; }

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

        // Expand function-like macros at the token level, before parsing.
        if (options.enableMacros)
        {
            tokens = compiler::expandMacroTokens(tokens, errorHandler);
            if (errorHandler.hasFatalErrors())
            {
                return false;
            }
        }

        // Parsing
        parser::Parser parser(tokens);
        ast::StmtPtr program = parser.parse();

        if (errorHandler.hasFatalErrors() || !program)
        {
            return false;
        }

        // Resolve `import` statements by loading and merging other modules.
        program = resolveImports(program, filename);
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

    /**
     * @brief Resolve `import` statements by loading, parsing and merging the
     *        imported modules' declarations into a single program.
     */
    ast::StmtPtr resolveImports(ast::StmtPtr program, const std::string& mainFile)
    {
        namespace fs = std::filesystem;
        std::set<std::string> loaded;
        std::vector<ast::StmtPtr> merged;

        auto flatten = [](ast::StmtPtr p) {
            std::vector<ast::StmtPtr> v;
            if (auto blk = std::dynamic_pointer_cast<ast::BlockStmt>(p)) v = blk->statements;
            else if (auto mod = std::dynamic_pointer_cast<ast::ModuleStmt>(p)) v = mod->body;
            else if (p) v.push_back(p);
            return v;
        };

        auto resolvePath = [&](const std::string& name, const std::string& fromDir) -> std::string {
            std::string rel = name;
            if (rel.size() < 3 || rel.substr(rel.size() - 3) != ".to") rel += ".to";
            std::vector<std::string> candidates;
            if (!fromDir.empty()) candidates.push_back((fs::path(fromDir) / rel).string());
            if (const char* envp = std::getenv("TOCIN_PATH"))
                candidates.push_back((fs::path(envp) / rel).string());
#ifdef TOCIN_STDLIB_PATH
            candidates.push_back((fs::path(TOCIN_STDLIB_PATH) / rel).string());
#endif
            for (auto& c : candidates)
            {
                std::error_code ec;
                if (fs::exists(c, ec)) return fs::absolute(c, ec).string();
            }
            return "";
        };

        std::function<void(ast::StmtPtr, const std::string&)> process =
            [&](ast::StmtPtr prog, const std::string& fromDir) {
            for (auto& s : flatten(prog))
            {
                if (auto imp = std::dynamic_pointer_cast<ast::ImportStmt>(s))
                {
                    std::string file = resolvePath(imp->moduleName, fromDir);
                    if (file.empty())
                    {
                        errorHandler.reportError(error::ErrorCode::I001_FILE_NOT_FOUND,
                                                 "Cannot resolve import '" + imp->moduleName + "'",
                                                 mainFile, 0, 0);
                        continue;
                    }
                    if (loaded.count(file)) continue;
                    loaded.insert(file);
                    std::ifstream f(file);
                    std::string src((std::istreambuf_iterator<char>(f)),
                                    std::istreambuf_iterator<char>());
                    lexer::Lexer lx(src, file, 4);
                    auto toks = lx.tokenize();
                    parser::Parser ps(toks);
                    auto sub = ps.parse();
                    process(sub, fs::path(file).parent_path().string()); // imported module's imports first
                }
                else if (s)
                {
                    merged.push_back(s);
                }
            }
        };

        process(program, fs::path(mainFile).parent_path().string());
        return std::make_shared<ast::BlockStmt>(
            lexer::Token(lexer::TokenType::IDENTIFIER, "", mainFile, 0, 0), merged);
    }

    bool compileToNative(ast::StmtPtr program, const std::string& filename,
                        const CompilationOptions& options)
    {
        // IR generation. The context is heap-allocated so it can be handed to
        // the JIT (which needs to own the context alongside the module).
        auto context = std::make_unique<llvm::LLVMContext>();
        auto module = std::make_unique<llvm::Module>(filename, *context);

        codegen::IRGenerator generator(*context, std::move(module), errorHandler);

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

        // JIT execution takes precedence: run the program in-process.
        if (options.run)
        {
            return runJIT(std::move(context), std::move(generatedModule), filename);
        }

        // Write output if specified.
        if (!options.outputFile.empty())
        {
            const std::string& outputPath = options.outputFile;
            auto endsWith = [](const std::string& s, const std::string& suffix) {
                return s.size() >= suffix.size() &&
                       s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
            };

            if (endsWith(outputPath, ".ll"))
            {
                std::error_code EC;
                llvm::raw_fd_ostream out(outputPath, EC);
                if (EC)
                {
                    errorHandler.reportError(error::ErrorCode::I003_READ_ERROR,
                                             "Could not open output file: " + EC.message(),
                                             filename, 0, 0);
                    return false;
                }
                out << *generatedModule;
            }
            else if (endsWith(outputPath, ".s"))
            {
                if (!emitObjectFile(*generatedModule, outputPath, /*asAssembly=*/true))
                    return false;
            }
            else if (endsWith(outputPath, ".o"))
            {
                if (!emitObjectFile(*generatedModule, outputPath, /*asAssembly=*/false))
                    return false;
            }
            else
            {
                // Produce a native executable: emit a temporary object file
                // and link it with the system C toolchain.
                std::string objPath = outputPath + ".o";
                if (!emitObjectFile(*generatedModule, objPath, false))
                    return false;
                if (!linkExecutable(objPath, outputPath))
                    return false;
                std::remove(objPath.c_str());
            }
        }

        return !errorHandler.hasFatalErrors();
    }

    /**
     * @brief JIT-compile and execute the module's main() in-process.
     */
    bool runJIT(std::unique_ptr<llvm::LLVMContext> context,
                std::unique_ptr<llvm::Module> module,
                const std::string& filename)
    {
        auto jitOrErr = llvm::orc::LLJITBuilder().create();
        if (!jitOrErr)
        {
            errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                     "Failed to create JIT: " +
                                         llvm::toString(jitOrErr.takeError()),
                                     filename, 0, 0);
            return false;
        }
        auto jit = std::move(*jitOrErr);

        // Resolve external symbols (printf, malloc, ...) from this process.
        if (auto gen = llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(
                jit->getDataLayout().getGlobalPrefix()))
        {
            jit->getMainJITDylib().addGenerator(std::move(*gen));
        }

        // Register the Tocin runtime (channels/goroutines) so JIT'd code can
        // call it. Taking the addresses also forces these symbols to be linked.
        {
            llvm::orc::SymbolMap rt;
            auto def = [&](const char *name, void *addr) {
                rt[jit->mangleAndIntern(name)] = llvm::orc::ExecutorSymbolDef(
                    llvm::orc::ExecutorAddr::fromPtr(addr), llvm::JITSymbolFlags::Exported);
            };
            def("__tocin_chan_new", reinterpret_cast<void *>(&__tocin_chan_new));
            def("__tocin_chan_send", reinterpret_cast<void *>(&__tocin_chan_send));
            def("__tocin_chan_recv", reinterpret_cast<void *>(&__tocin_chan_recv));
            def("__tocin_go", reinterpret_cast<void *>(&__tocin_go));
            def("__tocin_join_all", reinterpret_cast<void *>(&__tocin_join_all));
            def("__tocin_chan_try_recv", reinterpret_cast<void *>(&__tocin_chan_try_recv));
            def("__tocin_chan_park", reinterpret_cast<void *>(&__tocin_chan_park));
            def("__tocin_try_register", reinterpret_cast<void *>(&__tocin_try_register));
            def("__tocin_try_pop", reinterpret_cast<void *>(&__tocin_try_pop));
            def("__tocin_exc_value", reinterpret_cast<void *>(&__tocin_exc_value));
            def("__tocin_throw", reinterpret_cast<void *>(&__tocin_throw));
            def("__tocin_vec_new", reinterpret_cast<void *>(&__tocin_vec_new));
            def("__tocin_vec_push", reinterpret_cast<void *>(&__tocin_vec_push));
            def("__tocin_vec_get", reinterpret_cast<void *>(&__tocin_vec_get));
            def("__tocin_vec_set", reinterpret_cast<void *>(&__tocin_vec_set));
            def("__tocin_vec_len", reinterpret_cast<void *>(&__tocin_vec_len));
            def("__tocin_vec_pop", reinterpret_cast<void *>(&__tocin_vec_pop));
            def("__tocin_vec_free", reinterpret_cast<void *>(&__tocin_vec_free));
            def("__tocin_map_new", reinterpret_cast<void *>(&__tocin_map_new));
            def("__tocin_map_put", reinterpret_cast<void *>(&__tocin_map_put));
            def("__tocin_map_get", reinterpret_cast<void *>(&__tocin_map_get));
            def("__tocin_map_has", reinterpret_cast<void *>(&__tocin_map_has));
            def("__tocin_map_put_str", reinterpret_cast<void *>(&__tocin_map_put_str));
            def("__tocin_map_get_str", reinterpret_cast<void *>(&__tocin_map_get_str));
            def("__tocin_map_has_str", reinterpret_cast<void *>(&__tocin_map_has_str));
            def("__tocin_map_len", reinterpret_cast<void *>(&__tocin_map_len));
            def("__tocin_map_free", reinterpret_cast<void *>(&__tocin_map_free));
            def("__tocin_str_len", reinterpret_cast<void *>(&__tocin_str_len));
            def("__tocin_str_char_at", reinterpret_cast<void *>(&__tocin_str_char_at));
            def("__tocin_str_substring", reinterpret_cast<void *>(&__tocin_str_substring));
            def("__tocin_str_eq", reinterpret_cast<void *>(&__tocin_str_eq));
            def("__tocin_str_cmp", reinterpret_cast<void *>(&__tocin_str_cmp));
            def("__tocin_str_index_of_char", reinterpret_cast<void *>(&__tocin_str_index_of_char));
            def("__tocin_int_to_str", reinterpret_cast<void *>(&__tocin_int_to_str));
            def("__tocin_str_to_int", reinterpret_cast<void *>(&__tocin_str_to_int));
            def("__tocin_char_to_str", reinterpret_cast<void *>(&__tocin_char_to_str));
            def("__tocin_str_concat", reinterpret_cast<void *>(&__tocin_str_concat));
            def("__tocin_str_to_upper", reinterpret_cast<void *>(&__tocin_str_to_upper));
            def("__tocin_str_to_lower", reinterpret_cast<void *>(&__tocin_str_to_lower));
            def("__tocin_str_index_of", reinterpret_cast<void *>(&__tocin_str_index_of));
            def("__tocin_str_contains", reinterpret_cast<void *>(&__tocin_str_contains));
            def("__tocin_str_starts_with", reinterpret_cast<void *>(&__tocin_str_starts_with));
            def("__tocin_str_ends_with", reinterpret_cast<void *>(&__tocin_str_ends_with));
            def("__tocin_free", reinterpret_cast<void *>(&__tocin_free));
            def("__tocin_alloc", reinterpret_cast<void *>(&__tocin_alloc));
            def("__tocin_realloc", reinterpret_cast<void *>(&__tocin_realloc));
            def("__tocin_str_to_float", reinterpret_cast<void *>(&__tocin_str_to_float));
            def("__tocin_float_to_str", reinterpret_cast<void *>(&__tocin_float_to_str));
            def("__tocin_read_file", reinterpret_cast<void *>(&__tocin_read_file));
            def("__tocin_write_file", reinterpret_cast<void *>(&__tocin_write_file));
            def("__tocin_append_file", reinterpret_cast<void *>(&__tocin_append_file));
            def("__tocin_read_line", reinterpret_cast<void *>(&__tocin_read_line));
            llvm::cantFail(jit->getMainJITDylib().define(llvm::orc::absoluteSymbols(std::move(rt))));
        }

        if (auto err = jit->addIRModule(
                llvm::orc::ThreadSafeModule(std::move(module), std::move(context))))
        {
            errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                     "Failed to add module to JIT: " +
                                         llvm::toString(std::move(err)),
                                     filename, 0, 0);
            return false;
        }

        auto mainSym = jit->lookup("main");
        if (!mainSym)
        {
            errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                     "No 'main' function to execute: " +
                                         llvm::toString(mainSym.takeError()),
                                     filename, 0, 0);
            return false;
        }

        auto* mainFn = mainSym->toPtr<int64_t (*)()>();
        programExitCode = static_cast<int>(mainFn());
        return true;
    }

    /**
     * @brief Emit a native object (or assembly) file from the module.
     */
    bool emitObjectFile(llvm::Module& module, const std::string& outputPath, bool asAssembly)
    {
        std::string triple = llvm::sys::getDefaultTargetTriple();
        module.setTargetTriple(triple);

        std::string error;
        const llvm::Target* target = llvm::TargetRegistry::lookupTarget(triple, error);
        if (!target)
        {
            errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                     "Failed to look up target: " + error, "", 0, 0);
            return false;
        }

        llvm::TargetOptions opt;
        auto targetMachine = target->createTargetMachine(
            triple, "generic", "", opt, std::optional<llvm::Reloc::Model>(llvm::Reloc::PIC_));
        module.setDataLayout(targetMachine->createDataLayout());

        std::error_code EC;
        llvm::raw_fd_ostream dest(outputPath, EC, llvm::sys::fs::OF_None);
        if (EC)
        {
            errorHandler.reportError(error::ErrorCode::I003_READ_ERROR,
                                     "Could not open output file: " + EC.message(), "", 0, 0);
            return false;
        }

        llvm::legacy::PassManager pass;
        auto fileType = asAssembly ? llvm::CodeGenFileType::AssemblyFile
                                   : llvm::CodeGenFileType::ObjectFile;
        if (targetMachine->addPassesToEmitFile(pass, dest, nullptr, fileType))
        {
            errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                     "Target machine cannot emit this file type", "", 0, 0);
            return false;
        }
        pass.run(module);
        dest.flush();
        return true;
    }

    /**
     * @brief Link an object file into a native executable using the system C toolchain.
     */
    bool linkExecutable(const std::string& objPath, const std::string& exePath)
    {
        std::string cc = std::getenv("CC") ? std::getenv("CC") : "cc";
        std::string cmd = cc + " \"" + objPath + "\" -o \"" + exePath + "\"";
        // Link the Tocin runtime (channels/goroutines) so programs that use
        // concurrency resolve their __tocin_* calls.
#ifdef TOCIN_RUNTIME_LIB
        cmd += std::string(" \"") + TOCIN_RUNTIME_LIB + "\"";
#endif
        // Link the garbage collector so collected allocations resolve at runtime.
#ifdef TOCIN_GC_LIB
        cmd += std::string(" \"") + TOCIN_GC_LIB + "\"";
#endif
        cmd += " -lm -lpthread -lstdc++";
        int rc = std::system(cmd.c_str());
        if (rc != 0)
        {
            errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                     "Linker failed (exit " + std::to_string(rc) +
                                         "): " + cmd,
                                     "", 0, 0);
            return false;
        }
        return true;
    }

    bool compileToWASM(ast::StmtPtr program, const std::string& filename,
                       const CompilationOptions& options)
    {
#ifdef WITH_WASM
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
#else
        (void)program; (void)filename; (void)options;
        return false;
#endif
    }

    // Package manager methods
    bool installPackage(const std::string& name, const std::string& version = "") {
#ifdef WITH_PACKAGE_MANAGER
        return packageManager->install(name, version);
#else
        return false;
#endif
    }

    bool uninstallPackage(const std::string& name) {
#ifdef WITH_PACKAGE_MANAGER
        return packageManager->uninstall(name);
#else
        return false;
#endif
    }

#ifdef WITH_PACKAGE_MANAGER
    std::vector<package::PackageInfo> searchPackages(const std::string& query) {
        return packageManager->search(query);
    }
#endif

    // Debugger methods
    void startDebugger() {
#ifdef WITH_DEBUGGER
        if (debugger) {
            debugger->start();
        }
#endif
    }

    void setBreakpoint(const std::string& filename, int line, int column = 0) {
#ifdef WITH_DEBUGGER
        if (debugger) {
            debugger->setBreakpoint(filename, line, column);
        }
#endif
    }

    void stepInto() {
#ifdef WITH_DEBUGGER
        if (debugger) {
            debugger->stepInto();
        }
#endif
    }

    void stepOver() {
#ifdef WITH_DEBUGGER
        if (debugger) {
            debugger->stepOver();
        }
#endif
    }

    void continueExecution() {
#ifdef WITH_DEBUGGER
        if (debugger) {
            debugger->continueExecution();
        }
#endif
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
#ifdef WITH_DEBUGGER
    std::unique_ptr<debugger::Debugger> debugger;
#endif
#ifdef WITH_WASM
    std::unique_ptr<targets::WASMTarget> wasmTarget;
#endif
#ifdef WITH_PACKAGE_MANAGER
    std::unique_ptr<package::PackageManager> packageManager;
#endif

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
              << "  --jit, --run           JIT-compile and run the program immediately\n"
              << "  -O0, -O1, -O2, -O3     Set optimization level (default: -O2)\n"
              << "  -o <file>              Write output to <file>. Extension selects format:\n"
              << "                           .ll = LLVM IR, .s = assembly, .o = object,\n"
              << "                           anything else = native executable\n"
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
        else if (arg == "--jit" || arg == "--run")
        {
            options.run = true;
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

    // When JIT-executing, propagate the program's exit code.
    if (options.run)
    {
        return compiler.getProgramExitCode();
    }

    return 0;
}
