#ifndef COMPILATION_CONTEXT_H
#define COMPILATION_CONTEXT_H

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>
#include <mutex>

namespace tocin {
namespace compiler {

/**
 * @brief Compilation context that manages the state and configuration of the compilation process.
 * Supports hot hybrid compilation for JIT execution.
 */
class CompilationContext {
public:
    CompilationContext(const std::string& filename);
    ~CompilationContext() = default;

    // File and module management
    const std::string& getFilename() const { return filename_; }
    void setFilename(const std::string& filename) { filename_ = filename; }
    
    // Module management
    const std::string& getCurrentModule() const { return currentModule_; }
    void setCurrentModule(const std::string& moduleName) { currentModule_ = moduleName; }
    
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
    
    // Symbol table management
    void addSymbol(const std::string& name, void* symbol);
    void* getSymbol(const std::string& name) const;
    bool hasSymbol(const std::string& name) const;
    
    // Module dependencies
    void addDependency(const std::string& moduleName);
    const std::unordered_set<std::string>& getDependencies() const { return dependencies_; }
    
    // Compilation state
    bool isCompiling() const { return isCompiling_; }
    void setCompiling(bool compiling) { isCompiling_ = compiling; }
    
    // Error tracking
    void addError(const std::string& error);
    const std::vector<std::string>& getErrors() const { return errors_; }
    void clearErrors() { errors_.clear(); }
    
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
    
    // Symbol table
    std::unordered_map<std::string, void*> symbols_;
    
    // Module dependencies
    std::unordered_set<std::string> dependencies_;
    
    // Error tracking
    std::vector<std::string> errors_;
    
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
