#include "../include/optimization_passes.h"
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Utils.h>
#include <llvm/Analysis/TargetTransformInfo.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Transforms/Scalar/SimplifyCFG.h>
#include <llvm/Transforms/Scalar/Reassociate.h>
#include <llvm/Transforms/Scalar/GVN.h>
#include <llvm/Transforms/Scalar/NewGVN.h>
#include <llvm/Analysis/CGSCCPassManager.h>
#include <llvm/Analysis/LoopAnalysisManager.h>
#include <chrono>
#include <iostream>

using namespace llvm;
using namespace quill;

QuillOptimizationManager::QuillOptimizationManager(OptimizationLevel level) 
    : opt_level(level) {
    setupPassPipeline();
}

void QuillOptimizationManager::runOptimizations(Module& module) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Reset stats
    stats = OptimizationStats{};
    
    // Run module-level optimizations
    if (module_pm) {
        ModuleAnalysisManager MAM;
        PassBuilder PB;
        PB.registerModuleAnalyses(MAM);
        
        module_pm->run(module, MAM);
    }
    
    // Run function-level optimizations
    if (function_pm) {
        FunctionAnalysisManager FAM;
        ModuleAnalysisManager MAM;
        CGSCCAnalysisManager CGAM;
        LoopAnalysisManager LAM;
        
        PassBuilder PB;
        PB.registerModuleAnalyses(MAM);
        PB.registerCGSCCAnalyses(CGAM);  
        PB.registerFunctionAnalyses(FAM);
        PB.registerLoopAnalyses(LAM);
        PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);
        
        for (Function& F : module) {
            if (!F.isDeclaration()) {
                function_pm->run(F, FAM);
            }
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
    stats.optimization_time_ms = duration.count() / 1000000.0;
    
    // Collect statistics from type-directed pass if available
    if (type_directed_pass) {
        const auto& type_stats = type_directed_pass->getStats();
        stats.type_specializations = type_stats.specializations_applied;
        stats.type_casts_eliminated = type_stats.type_casts_eliminated;
        stats.numeric_operations_optimized = type_stats.numeric_optimizations;
        stats.divisions_to_shifts = type_stats.divisions_to_shifts;
        stats.multiplications_to_shifts = type_stats.multiplication_to_shifts;
    }
}

void QuillOptimizationManager::setOptimizationLevel(OptimizationLevel level) {
    opt_level = level;
    setupPassPipeline();
}

void QuillOptimizationManager::setupPassPipeline() {
    function_pm = std::make_unique<FunctionPassManager>();
    module_pm = std::make_unique<ModulePassManager>();
    
    switch (opt_level) {
        case O0:
            // No optimizations
            break;
            
        case O1:
            addBasicOptimizations();
            break;
            
        case O2:
            addBasicOptimizations();
            addAdvancedOptimizations();
            break;
            
        case O3:
            addBasicOptimizations();
            addAdvancedOptimizations();
            function_pm->addPass(QuillArithmeticSimplificationPass());
            type_directed_pass = std::make_unique<QuillTypeDirectedOptimizationPass>();
            function_pm->addPass(*type_directed_pass);
            break;
    }
}

void QuillOptimizationManager::addBasicOptimizations() {
    function_pm->addPass(InstCombinePass());
    function_pm->addPass(SimplifyCFGPass());
}

void QuillOptimizationManager::addAdvancedOptimizations() {
    function_pm->addPass(ReassociatePass());
    function_pm->addPass(GVNPass());
}

void QuillOptimizationManager::enablePass(const std::string& pass_name) {
    // For future extensibility - allow enabling specific passes
    // This would require a registry of available passes
}

void QuillOptimizationManager::disablePass(const std::string& pass_name) {
    // For future extensibility - allow disabling specific passes
}

void QuillOptimizationManager::printOptimizationReport() const {
    std::cout << "\n=== Quill Optimization Report ===" << std::endl;
    std::cout << "Optimization Level: O" << (int)opt_level << std::endl;
    std::cout << "Optimization Time: " << stats.optimization_time_ms << " ms" << std::endl;
    std::cout << "Instructions Eliminated: " << stats.instructions_eliminated << std::endl;
    std::cout << "Constants Folded: " << stats.constants_folded << std::endl;
    std::cout << "Functions Inlined: " << stats.functions_inlined << std::endl;
    std::cout << "Loops Optimized: " << stats.loops_optimized << std::endl;
    
    // Type-directed optimization statistics
    if (opt_level >= O3) {
        std::cout << "\n--- Type-Directed Optimizations ---" << std::endl;
        std::cout << "Numeric Operations Optimized: " << stats.numeric_operations_optimized << std::endl;
        std::cout << "Multiplications → Bit Shifts: " << stats.multiplications_to_shifts << std::endl;
        std::cout << "Divisions → Bit Shifts: " << stats.divisions_to_shifts << std::endl;
        std::cout << "Type Casts Eliminated: " << stats.type_casts_eliminated << std::endl;
        std::cout << "Type Specializations Applied: " << stats.type_specializations << std::endl;
    }
    std::cout << "==================================" << std::endl;
}