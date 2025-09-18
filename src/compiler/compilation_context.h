#ifndef COMPILATION_CONTEXT_H
#define COMPILATION_CONTEXT_H

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>
#include <mutex>
#include <chrono>
#include "../ast/types.h"

namespace tocin {
namespace compiler {

/**
 * @brief Compilation context that manages the state and configuration of the compilation process.
 * Supports hot hybrid compilation for JIT execution.
 */
class CompilationContext {
public:
    // Symbol information
    struct Symbol {
        std::string name;
        ast::TypePtr type;
        bool isConstant;
        bool isGlobal;
        size_t scopeLevel;
        
        Symbol() = default;
        Symbol(const std::string& n, ast::TypePtr t, bool c = false, bool g = false)
            : name(n), type(t), isConstant(c), isGlobal(g), scopeLevel(0) {}
    };
    
    // Function information
    struct FunctionInfo {
        std::string name;
        ast::TypePtr returnType;
        std::vector<ast::TypePtr> parameterTypes;
        std::vector<std::string> parameterNames;
        bool isVariadic;
        bool isGeneric;
        std::vector<std::string> genericParams;
        
        FunctionInfo() = default;
    };
    
    // Class information
    struct ClassInfo {
        std::string name;
        ast::TypePtr type;
        std::vector<std::string> memberNames;
        std::vector<ast::TypePtr> memberTypes;
        std::vector<FunctionInfo> methods;
        bool isAbstract;
        bool isGeneric;
        std::vector<std::string> genericParams;
        
        ClassInfo() = default;
    };
    
    // Trait information
    struct TraitInfo {
        std::string name;
        std::vector<FunctionInfo> requiredMethods;
        std::vector<std::string> genericParams;
        
        TraitInfo() = default;
    };
    
    // Module information
    struct ModuleInfo {
        std::string name;
        std::string path;
        bool isLoaded;
        std::vector<std::string> exports;
        
        ModuleInfo() = default;
    };
    
    // Generic instantiation
    struct GenericInstantiation {
        std::string baseName;
        std::vector<ast::TypePtr> typeArguments;
        ast::TypePtr instantiatedType;
        
        GenericInstantiation() = default;
    };

    CompilationContext(const std::string& filename);
    ~CompilationContext();

    // File and module management
    const std::string& getFilename() const { return filename_; }
    void setFilename(const std::string& filename) { filename_ = filename; }
    
    // Module management
    const std::string& getCurrentModule() const { return currentModule_; }
    void setCurrentModule(const std::string& moduleName) { currentModule_ = moduleName; }
    
    // Scope management
    void enterScope();
    void exitScope();
    size_t getCurrentScopeLevel() const { return currentScopeLevel_; }
    
    // Symbol table management
    bool declareSymbol(const std::string& name, ast::TypePtr type, bool isConstant = false);
    bool declareSymbol(const Symbol& symbol);
    Symbol* lookupSymbol(const std::string& name);
    const Symbol* lookupSymbol(const std::string& name) const;
    bool isSymbolDeclared(const std::string& name) const;
    
    // Function management
    bool declareFunction(const FunctionInfo& function);
    FunctionInfo* lookupFunction(const std::string& name);
    const FunctionInfo* lookupFunction(const std::string& name) const;
    std::vector<FunctionInfo*> lookupOverloadedFunctions(const std::string& name);
    
    // Class management
    bool declareClass(const ClassInfo& classInfo);
    ClassInfo* lookupClass(const std::string& name);
    const ClassInfo* lookupClass(const std::string& name) const;
    
    // Trait management
    bool declareTrait(const TraitInfo& traitInfo);
    TraitInfo* lookupTrait(const std::string& name);
    const TraitInfo* lookupTrait(const std::string& name) const;
    
    // Module management
    bool importModule(const std::string& moduleName, const std::string& path);
    ModuleInfo* lookupModule(const std::string& name);
    const ModuleInfo* lookupModule(const std::string& name) const;
    
    // Generic type instantiation
    bool registerGenericInstantiation(const GenericInstantiation& instantiation);
    ast::TypePtr lookupGenericInstantiation(const std::string& baseName, 
                                          const std::vector<ast::TypePtr>& typeArguments);
    
    // Error and warning management
    void addError(const std::string& message, size_t line = 0, size_t column = 0);
    void addWarning(const std::string& message, size_t line = 0, size_t column = 0);
    const std::vector<std::string>& getErrors() const { return errors_; }
    const std::vector<std::string>& getWarnings() const { return warnings_; }
    void clearErrors() { errors_.clear(); }
    void clearWarnings() { warnings_.clear(); }
    
    // Hot hybrid compilation support
    bool isHotHybridEnabled() const { return hotHybridEnabled_; }
    void setHotHybridEnabled(bool enabled) { hotHybridEnabled_ = enabled; }
    
    // JIT compilation settings
    bool isJITEnabled() const { return jitEnabled_; }
    void setJITEnabled(bool enabled) { jitEnabled_ = enabled; }
    
    // Optimization settings
    int getOptimizationLevel() const { return optimizationLevel_; }
    void setOptimizationLevel(int level) { optimizationLevel_ = level; }
    
    // Feature flags
    bool isFFIEnabled() const { return ffiEnabled_; }
    void setFFIEnabled(bool enabled) { ffiEnabled_ = enabled; }
    
    bool isConcurrencyEnabled() const { return concurrencyEnabled_; }
    void setConcurrencyEnabled(bool enabled) { concurrencyEnabled_ = enabled; }
    
    bool isAdvancedFeaturesEnabled() const { return advancedFeaturesEnabled_; }
    void setAdvancedFeaturesEnabled(bool enabled) { advancedFeaturesEnabled_ = enabled; }
    
    // Symbol table management (low-level)
    void addSymbol(const std::string& name, void* symbol);
    void* getSymbol(const std::string& name) const;
    bool hasSymbol(const std::string& name) const;
    
    // Module dependencies
    void addDependency(const std::string& dependency);
    const std::vector<std::string>& getDependencies() const { return dependencies_; }
    
    // Compilation state
    bool isCompiling() const { return isCompiling_; }
    void setCompiling(bool compiling) { isCompiling_ = compiling; }
    
    // Error tracking (low-level)
    void addError(const std::string& error);
    
    // Performance tracking
    void startTimer(const std::string& phase);
    double endTimer(const std::string& phase);
    const std::unordered_map<std::string, double>& getTimings() const { return timings_; }
    
    // Hot reload support
    void markForHotReload(const std::string& symbol);
    const std::unordered_set<std::string>& getHotReloadSymbols() const { return hotReloadSymbols_; }
    void clearHotReloadSymbols() { hotReloadSymbols_.clear(); }
    
    // Thread safety
    void lock() { mutex_.lock(); }
    void unlock() { mutex_.unlock(); }
    std::unique_lock<std::mutex> getLock() { return std::unique_lock<std::mutex>(mutex_); }

private:
    std::string filename_;
    std::string currentModule_;
    
    // Scope management
    size_t currentScopeLevel_ = 0;
    std::unordered_map<size_t, std::unordered_map<std::string, Symbol>> symbolTables_;
    
    // Function, class, trait, and module tables
    std::unordered_map<std::string, FunctionInfo> functions_;
    std::unordered_map<std::string, std::vector<FunctionInfo>> overloadedFunctions_;
    std::unordered_map<std::string, ClassInfo> classes_;
    std::unordered_map<std::string, TraitInfo> traits_;
    std::unordered_map<std::string, ModuleInfo> modules_;
    
    // Generic instantiations
    std::vector<GenericInstantiation> genericInstantiations_;
    
    // Hot hybrid compilation settings
    bool hotHybridEnabled_ = true;
    bool jitEnabled_ = true;
    int optimizationLevel_ = 2;
    
    // Feature flags
    bool ffiEnabled_ = true;
    bool concurrencyEnabled_ = true;
    bool advancedFeaturesEnabled_ = true;
    
    // Compilation state
    bool isCompiling_ = false;
    
    // Symbol table (low-level)
    std::unordered_map<std::string, void*> symbols_;
    
    // Module dependencies
    std::vector<std::string> dependencies_;
    
    // Error tracking
    std::vector<std::string> errors_;
    std::vector<std::string> warnings_;
    
    // Performance tracking
    std::unordered_map<std::string, std::chrono::high_resolution_clock::time_point> timers_;
    std::unordered_map<std::string, double> timings_;
    
    // Hot reload support
    std::unordered_set<std::string> hotReloadSymbols_;
    
    // Thread safety
    mutable std::mutex mutex_;
};

} // namespace compiler
} // namespace tocin

#endif // COMPILATION_CONTEXT_H
