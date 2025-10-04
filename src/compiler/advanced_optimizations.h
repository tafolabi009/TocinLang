#pragma once

#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Analysis/ProfileSummaryInfo.h>
#include <memory>
#include <string>
#include <vector>

namespace tocin {
namespace optimization {

/**
 * @brief Profile-Guided Optimization (PGO) Manager
 * 
 * Collects runtime profiling data and uses it to guide optimizations
 * for better performance.
 */
class PGOManager {
public:
    PGOManager();
    ~PGOManager();

    // Profile collection
    void enableProfiling(llvm::Module* module);
    void disableProfiling();
    bool loadProfile(const std::string& profilePath);
    bool saveProfile(const std::string& profilePath);

    // Apply PGO optimizations
    void applyPGO(llvm::Module* module);

    // Statistics
    struct ProfileStats {
        size_t hotFunctions;
        size_t coldFunctions;
        size_t totalExecutions;
        size_t branchMispredictions;
    };
    
    ProfileStats getStats() const { return stats_; }

private:
    ProfileStats stats_;
    bool profilingEnabled_;
    std::string profileData_;
};

/**
 * @brief Interprocedural Optimization Manager
 * 
 * Performs optimizations across function boundaries for better
 * global optimization.
 */
class InterproceduralOptimizer {
public:
    InterproceduralOptimizer();
    ~InterproceduralOptimizer();

    // Optimization passes
    void optimizeCallGraph(llvm::Module* module);
    void performInlining(llvm::Module* module, int inlineThreshold = 225);
    void performDevirtualization(llvm::Module* module);
    void performConstantPropagation(llvm::Module* module);

    // Analysis
    struct CallGraphStats {
        size_t totalFunctions;
        size_t inlinedFunctions;
        size_t devirtualizedCalls;
        size_t constantsPropagated;
    };
    
    CallGraphStats getStats() const { return stats_; }

private:
    CallGraphStats stats_;
};

/**
 * @brief Polyhedral Loop Optimizer
 * 
 * Advanced loop transformations using polyhedral analysis
 * for automatic parallelization and cache optimization.
 */
class PolyhedralOptimizer {
public:
    PolyhedralOptimizer();
    ~PolyhedralOptimizer();

    // Loop transformations
    void analyzeLoops(llvm::Module* module);
    void applyLoopFusion(llvm::Module* module);
    void applyLoopTiling(llvm::Module* module, size_t tileSize = 64);
    void applyLoopInterchange(llvm::Module* module);
    void applyVectorization(llvm::Module* module);

    // Parallelization
    void detectParallelLoops(llvm::Module* module);
    void generateParallelCode(llvm::Module* module);

    struct LoopStats {
        size_t totalLoops;
        size_t fusedLoops;
        size_t tiledLoops;
        size_t vectorizedLoops;
        size_t parallelLoops;
    };
    
    LoopStats getStats() const { return stats_; }

private:
    LoopStats stats_;
};

/**
 * @brief Whole-Program Optimization Manager
 * 
 * Orchestrates optimization across entire program including
 * link-time optimization (LTO).
 */
class WholeProgramOptimizer {
public:
    WholeProgramOptimizer();
    ~WholeProgramOptimizer();

    // Setup
    void addModule(llvm::Module* module);
    void setOptimizationLevel(int level); // 0-3

    // Optimization phases
    void performLTO();
    void eliminateDeadCode();
    void performGlobalValueNumbering();
    void optimizeGlobalVariables();

    // Full optimization pipeline
    void optimize();

    struct OptimizationStats {
        size_t modulesProcessed;
        size_t functionsEliminated;
        size_t globalsOptimized;
        size_t bytesReduced;
    };
    
    OptimizationStats getStats() const { return stats_; }

private:
    std::vector<llvm::Module*> modules_;
    int optimizationLevel_;
    OptimizationStats stats_;
};

/**
 * @brief Advanced Optimization Pipeline
 * 
 * Integrates all advanced optimization techniques in a coordinated
 * pipeline for maximum performance.
 */
class AdvancedOptimizationPipeline {
public:
    AdvancedOptimizationPipeline();
    ~AdvancedOptimizationPipeline();

    // Configuration
    void enablePGO(bool enable);
    void enableIPO(bool enable);
    void enablePolyhedral(bool enable);
    void enableLTO(bool enable);
    void setOptimizationLevel(int level);

    // Execute optimization pipeline
    void optimize(llvm::Module* module);

    // Statistics
    struct PipelineStats {
        PGOManager::ProfileStats pgoStats;
        InterproceduralOptimizer::CallGraphStats ipoStats;
        PolyhedralOptimizer::LoopStats loopStats;
        WholeProgramOptimizer::OptimizationStats wpoStats;
        double optimizationTimeMs;
    };
    
    PipelineStats getStats() const { return stats_; }

private:
    std::unique_ptr<PGOManager> pgo_;
    std::unique_ptr<InterproceduralOptimizer> ipo_;
    std::unique_ptr<PolyhedralOptimizer> polyhedral_;
    std::unique_ptr<WholeProgramOptimizer> wpo_;
    
    bool pgoEnabled_;
    bool ipoEnabled_;
    bool polyhedralEnabled_;
    bool ltoEnabled_;
    int optimizationLevel_;
    
    PipelineStats stats_;
};

} // namespace optimization
} // namespace tocin
