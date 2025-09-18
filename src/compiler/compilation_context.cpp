#include "compilation_context.h"
#include <chrono>
#include <iostream>
#include <algorithm>

namespace tocin {
namespace compiler {

CompilationContext::CompilationContext(const std::string& filename)
    : filename_(filename), currentModule_("main"), hotHybridEnabled_(true),
      jitEnabled_(true), optimizationLevel_(2), ffiEnabled_(true),
      concurrencyEnabled_(true), advancedFeaturesEnabled_(true), isCompiling_(false) {
}

CompilationContext::~CompilationContext() = default;

void CompilationContext::enterScope() {
    currentScopeLevel_++;
    symbolTables_[currentScopeLevel_] = std::unordered_map<std::string, Symbol>();
}

void CompilationContext::exitScope() {
    if (currentScopeLevel_ > 0) {
        symbolTables_.erase(currentScopeLevel_);
        currentScopeLevel_--;
    }
}

bool CompilationContext::declareSymbol(const std::string& name, ast::TypePtr type, bool isConstant) {
    Symbol symbol(name, type, isConstant, currentScopeLevel_ == 0);
    symbol.scopeLevel = currentScopeLevel_;
    return declareSymbol(symbol);
}

bool CompilationContext::declareSymbol(const Symbol& symbol) {
    auto& currentScope = symbolTables_[currentScopeLevel_];
    if (currentScope.find(symbol.name) != currentScope.end()) {
        addError("Symbol '" + symbol.name + "' already declared in current scope", 0, 0);
        return false;
    }
    currentScope[symbol.name] = symbol;
    return true;
}

CompilationContext::Symbol* CompilationContext::lookupSymbol(const std::string& name) {
    // Search from current scope up to global scope
    for (int level = static_cast<int>(currentScopeLevel_); level >= 0; --level) {
        auto scopeIt = symbolTables_.find(level);
        if (scopeIt != symbolTables_.end()) {
            auto symbolIt = scopeIt->second.find(name);
            if (symbolIt != scopeIt->second.end()) {
                return &symbolIt->second;
            }
        }
    }
    return nullptr;
}

const CompilationContext::Symbol* CompilationContext::lookupSymbol(const std::string& name) const {
    // Search from current scope up to global scope
    for (int level = static_cast<int>(currentScopeLevel_); level >= 0; --level) {
        auto scopeIt = symbolTables_.find(level);
        if (scopeIt != symbolTables_.end()) {
            auto symbolIt = scopeIt->second.find(name);
            if (symbolIt != scopeIt->second.end()) {
                return &symbolIt->second;
            }
        }
    }
    return nullptr;
}

bool CompilationContext::isSymbolDeclared(const std::string& name) const {
    return lookupSymbol(name) != nullptr;
}

bool CompilationContext::declareFunction(const FunctionInfo& function) {
    if (functions_.find(function.name) != functions_.end()) {
        // Check if this is an overload
        overloadedFunctions_[function.name].push_back(function);
    } else {
        functions_[function.name] = function;
    }
    return true;
}

CompilationContext::FunctionInfo* CompilationContext::lookupFunction(const std::string& name) {
    auto it = functions_.find(name);
    return (it != functions_.end()) ? &it->second : nullptr;
}

const CompilationContext::FunctionInfo* CompilationContext::lookupFunction(const std::string& name) const {
    auto it = functions_.find(name);
    return (it != functions_.end()) ? &it->second : nullptr;
}

std::vector<CompilationContext::FunctionInfo*> CompilationContext::lookupOverloadedFunctions(const std::string& name) {
    std::vector<FunctionInfo*> result;
    auto it = overloadedFunctions_.find(name);
    if (it != overloadedFunctions_.end()) {
        for (auto& func : it->second) {
            result.push_back(&func);
        }
    }
    return result;
}

bool CompilationContext::declareClass(const ClassInfo& classInfo) {
    if (classes_.find(classInfo.name) != classes_.end()) {
        addError("Class '" + classInfo.name + "' already declared", 0, 0);
        return false;
    }
    classes_[classInfo.name] = classInfo;
    return true;
}

CompilationContext::ClassInfo* CompilationContext::lookupClass(const std::string& name) {
    auto it = classes_.find(name);
    return (it != classes_.end()) ? &it->second : nullptr;
}

const CompilationContext::ClassInfo* CompilationContext::lookupClass(const std::string& name) const {
    auto it = classes_.find(name);
    return (it != classes_.end()) ? &it->second : nullptr;
}

bool CompilationContext::declareTrait(const TraitInfo& traitInfo) {
    if (traits_.find(traitInfo.name) != traits_.end()) {
        addError("Trait '" + traitInfo.name + "' already declared", 0, 0);
        return false;
    }
    traits_[traitInfo.name] = traitInfo;
    return true;
}

CompilationContext::TraitInfo* CompilationContext::lookupTrait(const std::string& name) {
    auto it = traits_.find(name);
    return (it != traits_.end()) ? &it->second : nullptr;
}

const CompilationContext::TraitInfo* CompilationContext::lookupTrait(const std::string& name) const {
    auto it = traits_.find(name);
    return (it != traits_.end()) ? &it->second : nullptr;
}

bool CompilationContext::importModule(const std::string& moduleName, const std::string& path) {
    if (modules_.find(moduleName) != modules_.end()) {
        return true; // Already imported
    }
    
    ModuleInfo moduleInfo;
    moduleInfo.name = moduleName;
    moduleInfo.path = path;
    moduleInfo.isLoaded = false;
    
    modules_[moduleName] = moduleInfo;
    addDependency(moduleName);
    return true;
}

CompilationContext::ModuleInfo* CompilationContext::lookupModule(const std::string& name) {
    auto it = modules_.find(name);
    return (it != modules_.end()) ? &it->second : nullptr;
}

const CompilationContext::ModuleInfo* CompilationContext::lookupModule(const std::string& name) const {
    auto it = modules_.find(name);
    return (it != modules_.end()) ? &it->second : nullptr;
}

bool CompilationContext::registerGenericInstantiation(const GenericInstantiation& instantiation) {
    // Check if this instantiation already exists
    for (const auto& existing : genericInstantiations_) {
        if (existing.baseName == instantiation.baseName && 
            existing.typeArguments.size() == instantiation.typeArguments.size()) {
            bool same = true;
            for (size_t i = 0; i < existing.typeArguments.size(); ++i) {
                if (!existing.typeArguments[i]->equals(instantiation.typeArguments[i])) {
                    same = false;
                    break;
                }
            }
            if (same) {
                return true; // Already exists
            }
        }
    }
    
    genericInstantiations_.push_back(instantiation);
    return true;
}

ast::TypePtr CompilationContext::lookupGenericInstantiation(const std::string& baseName, 
                                                           const std::vector<ast::TypePtr>& typeArguments) {
    for (const auto& instantiation : genericInstantiations_) {
        if (instantiation.baseName == baseName && 
            instantiation.typeArguments.size() == typeArguments.size()) {
            bool match = true;
            for (size_t i = 0; i < typeArguments.size(); ++i) {
                if (!instantiation.typeArguments[i]->equals(typeArguments[i])) {
                    match = false;
                    break;
                }
            }
            if (match) {
                return instantiation.instantiatedType;
            }
        }
    }
    return nullptr;
}

void CompilationContext::addError(const std::string& message, size_t line, size_t column) {
    std::string fullMessage = filename_;
    if (line > 0) {
        fullMessage += ":" + std::to_string(line);
        if (column > 0) {
            fullMessage += ":" + std::to_string(column);
        }
    }
    fullMessage += ": error: " + message;
    errors_.push_back(fullMessage);
}

void CompilationContext::addWarning(const std::string& message, size_t line, size_t column) {
    std::string fullMessage = filename_;
    if (line > 0) {
        fullMessage += ":" + std::to_string(line);
        if (column > 0) {
            fullMessage += ":" + std::to_string(column);
        }
    }
    fullMessage += ": warning: " + message;
    warnings_.push_back(fullMessage);
}

void CompilationContext::addDependency(const std::string& dependency) {
    if (std::find(dependencies_.begin(), dependencies_.end(), dependency) == dependencies_.end()) {
        dependencies_.push_back(dependency);
    }
}

void CompilationContext::addSymbol(const std::string& name, void* symbol) {
    std::lock_guard<std::mutex> lock(mutex_);
    symbols_[name] = symbol;
}

void* CompilationContext::getSymbol(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = symbols_.find(name);
    return it != symbols_.end() ? it->second : nullptr;
}

bool CompilationContext::hasSymbol(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return symbols_.find(name) != symbols_.end();
}


void CompilationContext::addError(const std::string& error) {
    std::lock_guard<std::mutex> lock(mutex_);
    errors_.push_back(error);
}

void CompilationContext::startTimer(const std::string& phase) {
    std::lock_guard<std::mutex> lock(mutex_);
    timers_[phase] = std::chrono::high_resolution_clock::now();
}

double CompilationContext::endTimer(const std::string& phase) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = timers_.find(phase);
    if (it == timers_.end()) {
        return 0.0;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - it->second);
    double milliseconds = duration.count() / 1000.0;
    timings_[phase] = milliseconds;
    timers_.erase(it);
    return milliseconds;
}

void CompilationContext::markForHotReload(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(mutex_);
    hotReloadSymbols_.insert(symbol);
}

} // namespace compiler
} // namespace tocin
