#pragma once

#include "../ast/ast.h"
#include "../error/error_handler.h"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

namespace targets {

/**
 * @brief WebAssembly target configuration
 */
struct WASMTargetConfig {
    bool optimize;
    bool enableSIMD;
    bool enableThreads;
    bool enableExceptionHandling;
    bool enableGarbageCollection;
    std::string importMemory;
    std::string exportMemory;
    int memorySize;
    int maxMemorySize;
    
    WASMTargetConfig()
        : optimize(true), enableSIMD(true), enableThreads(false),
          enableExceptionHandling(true), enableGarbageCollection(true),
          memorySize(256), maxMemorySize(65536) {}
};

/**
 * @brief WebAssembly target
 */
class WASMTarget {
private:
    WASMTargetConfig config;
    std::unordered_map<std::string, std::string> imports;
    std::unordered_map<std::string, std::string> exports;
    std::vector<std::string> functions;
    std::vector<std::string> globals;
    std::vector<std::string> memories;
    std::vector<std::string> tables;
    
public:
    WASMTarget(const WASMTargetConfig& cfg = WASMTargetConfig());
    ~WASMTarget();
    
    /**
     * @brief Generate WebAssembly code from AST
     */
    std::string generateWASM(ast::StmtPtr ast, error::ErrorHandler& errorHandler);
    
    /**
     * @brief Add import
     */
    void addImport(const std::string& module, const std::string& name, const std::string& signature);
    
    /**
     * @brief Add export
     */
    void addExport(const std::string& name, const std::string& signature);
    
    /**
     * @brief Generate WASM module
     */
    std::string generateModule();
    
    /**
     * @brief Optimize WASM code
     */
    std::string optimizeWASM(const std::string& wasmCode);
    
    /**
     * @brief Validate WASM code
     */
    bool validateWASM(const std::string& wasmCode, error::ErrorHandler& errorHandler);
    
private:
    std::string generateFunction(ast::FunctionStmt* func);
    std::string generateExpression(ast::ExprPtr expr);
    std::string generateStatement(ast::StmtPtr stmt);
    std::string generateType(ast::TypePtr type);
    std::string generateLocalVariables(const std::vector<ast::Parameter>& params);
    std::string generateInstructions(ast::StmtPtr stmt);
};

} // namespace targets 