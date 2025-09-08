#pragma once
#include <llvm/IR/PassManager.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Constants.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <memory>

// Forward declarations
namespace llvm {
    class Value;
    class Instruction;
    class BasicBlock;
}

namespace quill {

// Constant Folding Pass
class QuillConstantFoldingPass : public llvm::PassInfoMixin<QuillConstantFoldingPass> {
public:
    llvm::PreservedAnalyses run(llvm::Function &F, llvm::FunctionAnalysisManager &AM);
    
private:
    bool foldBinaryOperations(llvm::Function &F);
    bool foldConstants(llvm::Function &F);
    llvm::Value* evaluateConstantExpression(llvm::BinaryOperator* binOp);
};

// Dead Code Elimination Pass
class QuillDeadCodeEliminationPass : public llvm::PassInfoMixin<QuillDeadCodeEliminationPass> {
public:
    llvm::PreservedAnalyses run(llvm::Function &F, llvm::FunctionAnalysisManager &AM);
    
private:
    bool eliminateDeadInstructions(llvm::Function &F);
    bool eliminateUnreachableBlocks(llvm::Function &F);
    bool isInstructionDead(llvm::Instruction* inst);
};

// Simple Function Inlining Pass
class QuillFunctionInliningPass : public llvm::PassInfoMixin<QuillFunctionInliningPass> {
public:
    llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &AM);
    
private:
    bool shouldInlineFunction(llvm::Function* func);
    bool inlineSmallFunctions(llvm::Module &M);
    int calculateInstructionCount(llvm::Function* func);
    static const int INLINE_THRESHOLD = 20; // Instructions
};

// Loop Optimization Pass  
class QuillLoopOptimizationPass : public llvm::PassInfoMixin<QuillLoopOptimizationPass> {
public:
    llvm::PreservedAnalyses run(llvm::Function &F, llvm::FunctionAnalysisManager &AM);
    
private:
    bool optimizeLoops(llvm::Function &F);
    bool unrollSmallLoops(llvm::Function &F);
    bool hoistLoopInvariants(llvm::Function &F);
};

// Arithmetic Simplification Pass
class QuillArithmeticSimplificationPass : public llvm::PassInfoMixin<QuillArithmeticSimplificationPass> {
public:
    llvm::PreservedAnalyses run(llvm::Function &F, llvm::FunctionAnalysisManager &AM);
    
private:
    bool simplifyArithmetic(llvm::Function &F);
    llvm::Value* simplifyExpression(llvm::BinaryOperator* binOp);
    bool isZero(llvm::Value* val);
    bool isOne(llvm::Value* val);
};

// Forward declaration for type system integration
class TypeChecker;

// Type-Directed Optimization Pass
class QuillTypeDirectedOptimizationPass : public llvm::PassInfoMixin<QuillTypeDirectedOptimizationPass> {
public:
    struct TypeOptimizationStats {
        int specializations_applied = 0;
        int type_casts_eliminated = 0;
        int numeric_optimizations = 0;
        int integer_arithmetic_optimized = 0;
        int division_to_shifts = 0;
        int multiplication_to_shifts = 0;
    };
    
    QuillTypeDirectedOptimizationPass();
    llvm::PreservedAnalyses run(llvm::Function &F, llvm::FunctionAnalysisManager &AM);
    
    // Access to statistics
    const TypeOptimizationStats& getStats() const { return stats; }
    void resetStats() { stats = TypeOptimizationStats{}; }
    
    // Type information integration
    void setTypeInformation(const TypeChecker* type_checker) { type_info = type_checker; }
    
private:
    bool optimizeNumericOperations(llvm::Function &F);
    bool eliminateUnnecessaryTypeCasts(llvm::Function &F);
    bool specializeGenericFunctions(llvm::Function &F);
    bool optimizePolymorphicCalls(llvm::Function &F);
    bool inlineMonomorphicFunctions(llvm::Function &F);
    
    // Enhanced optimizations with type information
    bool optimizeIntegerArithmetic(llvm::Function &F);
    bool optimizeDivisionToShifts(llvm::Function &F);
    bool optimizeMultiplicationToShifts(llvm::Function &F);
    
    // Utility methods
    bool isIntegerConstant(llvm::Value* val, int64_t& out_value);
    bool isFloatConstant(llvm::Value* val, double& out_value);
    bool canSpecializeFunction(llvm::Function* func);
    bool isPowerOfTwo(int64_t value);
    int getShiftAmount(int64_t power_of_two);
    
    // Statistics
    TypeOptimizationStats stats;
    
    // Type system integration
    const TypeChecker* type_info = nullptr;
};

// Optimization Pass Manager for Quill
class QuillOptimizationManager {
public:
    enum OptimizationLevel {
        O0 = 0,  // No optimization
        O1 = 1,  // Basic optimizations
        O2 = 2,  // More aggressive optimizations  
        O3 = 3   // Maximum optimization
    };
    
    QuillOptimizationManager(OptimizationLevel level = O0);
    
    void runOptimizations(llvm::Module& module);
    void setOptimizationLevel(OptimizationLevel level);
    void enablePass(const std::string& pass_name);
    void disablePass(const std::string& pass_name);
    
    // Performance reporting
    struct OptimizationStats {
        int instructions_eliminated = 0;
        int constants_folded = 0;
        int functions_inlined = 0;
        int loops_optimized = 0;
        double optimization_time_ms = 0.0;
        
        // Type-directed optimization stats
        int type_specializations = 0;
        int type_casts_eliminated = 0;
        int numeric_operations_optimized = 0;
        int divisions_to_shifts = 0;
        int multiplications_to_shifts = 0;
    };
    
    const OptimizationStats& getStats() const { return stats; }
    void printOptimizationReport() const;
    
private:
    OptimizationLevel opt_level;
    OptimizationStats stats;
    
    std::unique_ptr<llvm::FunctionPassManager> function_pm;
    std::unique_ptr<llvm::ModulePassManager> module_pm;
    
    // Reference to type-directed pass for statistics collection
    std::unique_ptr<QuillTypeDirectedOptimizationPass> type_directed_pass;
    
    void setupPassPipeline();
    void addBasicOptimizations();
    void addAdvancedOptimizations();
};

} // namespace quill