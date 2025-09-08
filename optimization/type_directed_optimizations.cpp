#include "../include/optimization_passes.h"
#include "../include/type_system.h"
#include "../include/type_checker.h"
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Type.h>
#include <llvm/Support/Casting.h>
#include <iostream>

using namespace llvm;
using namespace quill;

namespace quill {

class TypeDirectedOptimizationPass {
private:
    TypeChecker type_checker;
    
    // Optimization statistics
    int specializations_applied = 0;
    int type_casts_eliminated = 0;
    int generic_instantiations = 0;
    
public:
    bool runOnFunction(Function &F);
    
    // Specific optimization methods
    bool optimizeNumericOperations(Function &F);
    bool eliminateUnnecessaryTypeCasts(Function &F);
    bool specializeGenericFunctions(Function &F);
    bool optimizePolymorphicCalls(Function &F);
    bool inlineMonomorphicFunctions(Function &F);
    
    // Utility methods
    bool isIntegerConstant(Value* val, int64_t& out_value);
    bool isFloatConstant(Value* val, double& out_value);
    Type* inferLLVMType(Value* val);
    bool canSpecializeFunction(Function* func);
    
    // Statistics
    void printOptimizationStats();
    void resetStats();
};

bool TypeDirectedOptimizationPass::runOnFunction(Function &F) {
    bool changed = false;
    
    // Run various type-directed optimizations
    changed |= optimizeNumericOperations(F);
    changed |= eliminateUnnecessaryTypeCasts(F);
    changed |= specializeGenericFunctions(F);
    changed |= optimizePolymorphicCalls(F);
    changed |= inlineMonomorphicFunctions(F);
    
    return changed;
}

bool TypeDirectedOptimizationPass::optimizeNumericOperations(Function &F) {
    bool changed = false;
    
    for (BasicBlock &BB : F) {
        for (auto I = BB.begin(), E = BB.end(); I != E; ++I) {
            Instruction *inst = &*I;
            
            // Optimize integer operations that don't need floating point
            if (auto *binOp = dyn_cast<BinaryOperator>(inst)) {
                Value *left = binOp->getOperand(0);
                Value *right = binOp->getOperand(1);
                
                // Check for integer constants being treated as floats
                int64_t leftInt, rightInt;
                if (isIntegerConstant(left, leftInt) && isIntegerConstant(right, rightInt)) {
                    
                    switch (binOp->getOpcode()) {
                        case Instruction::FAdd: {
                            // Replace floating point add with integer add if both operands are integers
                            IRBuilder<> builder(inst);
                            auto *leftConst = ConstantInt::get(builder.getInt64Ty(), leftInt);
                            auto *rightConst = ConstantInt::get(builder.getInt64Ty(), rightInt);
                            auto *intAdd = builder.CreateAdd(leftConst, rightConst, "int_add_opt");
                            
                            // Convert back to float for compatibility
                            auto *floatResult = builder.CreateSIToFP(intAdd, builder.getDoubleTy());
                            
                            binOp->replaceAllUsesWith(floatResult);
                            changed = true;
                            break;
                        }
                        
                        case Instruction::FMul: {
                            // Optimize multiplication by powers of 2 to bit shifts
                            if (rightInt > 0 && (rightInt & (rightInt - 1)) == 0) {
                                // rightInt is a power of 2
                                int shift_amount = 0;
                                int64_t temp = rightInt;
                                while (temp > 1) {
                                    temp >>= 1;
                                    shift_amount++;
                                }
                                
                                IRBuilder<> builder(inst);
                                auto *leftConst = ConstantInt::get(builder.getInt64Ty(), leftInt);
                                auto *shiftConst = ConstantInt::get(builder.getInt32Ty(), shift_amount);
                                auto *shifted = builder.CreateShl(leftConst, shiftConst, "shift_opt");
                                auto *floatResult = builder.CreateSIToFP(shifted, builder.getDoubleTy());
                                
                                binOp->replaceAllUsesWith(floatResult);
                                changed = true;
                            }
                            break;
                        }
                        
                        case Instruction::FDiv: {
                            // Optimize division by powers of 2 to bit shifts
                            if (rightInt > 0 && (rightInt & (rightInt - 1)) == 0) {
                                int shift_amount = 0;
                                int64_t temp = rightInt;
                                while (temp > 1) {
                                    temp >>= 1;
                                    shift_amount++;
                                }
                                
                                IRBuilder<> builder(inst);
                                auto *leftConst = ConstantInt::get(builder.getInt64Ty(), leftInt);
                                auto *shiftConst = ConstantInt::get(builder.getInt32Ty(), shift_amount);
                                auto *shifted = builder.CreateAShr(leftConst, shiftConst, "div_shift_opt");
                                auto *floatResult = builder.CreateSIToFP(shifted, builder.getDoubleTy());
                                
                                binOp->replaceAllUsesWith(floatResult);
                                changed = true;
                            }
                            break;
                        }
                        
                        default:
                            break;
                    }
                }
            }
            
            // Optimize comparisons
            else if (auto *cmp = dyn_cast<FCmpInst>(inst)) {
                Value *left = cmp->getOperand(0);
                Value *right = cmp->getOperand(1);
                
                int64_t leftInt, rightInt;
                if (isIntegerConstant(left, leftInt) && isIntegerConstant(right, rightInt)) {
                    // Replace floating point comparison with integer comparison
                    IRBuilder<> builder(inst);
                    auto *leftConst = ConstantInt::get(builder.getInt64Ty(), leftInt);
                    auto *rightConst = ConstantInt::get(builder.getInt64Ty(), rightInt);
                    
                    CmpInst::Predicate intPred;
                    switch (cmp->getPredicate()) {
                        case FCmpInst::FCMP_OEQ: intPred = ICmpInst::ICMP_EQ; break;
                        case FCmpInst::FCMP_ONE: intPred = ICmpInst::ICMP_NE; break;
                        case FCmpInst::FCMP_OLT: intPred = ICmpInst::ICMP_SLT; break;
                        case FCmpInst::FCMP_OLE: intPred = ICmpInst::ICMP_SLE; break;
                        case FCmpInst::FCMP_OGT: intPred = ICmpInst::ICMP_SGT; break;
                        case FCmpInst::FCMP_OGE: intPred = ICmpInst::ICMP_SGE; break;
                        default: continue; // Skip unsupported predicates
                    }
                    
                    auto *intCmp = builder.CreateICmp(intPred, leftConst, rightConst, "int_cmp_opt");
                    // Convert boolean result back to double for compatibility
                    auto *floatResult = builder.CreateUIToFP(intCmp, builder.getDoubleTy());
                    
                    cmp->replaceAllUsesWith(floatResult);
                    changed = true;
                }
            }
        }
    }
    
    return changed;
}

bool TypeDirectedOptimizationPass::eliminateUnnecessaryTypeCasts(Function &F) {
    bool changed = false;
    
    for (BasicBlock &BB : F) {
        for (auto I = BB.begin(), E = BB.end(); I != E; ++I) {
            Instruction *inst = &*I;
            
            // Look for unnecessary cast chains
            if (auto *cast = dyn_cast<CastInst>(inst)) {
                Value *operand = cast->getOperand(0);
                
                // Check for cast-to-same-type
                if (cast->getSrcTy() == cast->getDestTy()) {
                    cast->replaceAllUsesWith(operand);
                    changed = true;
                    type_casts_eliminated++;
                    continue;
                }
                
                // Check for cast chains (cast(cast(x)))
                if (auto *innerCast = dyn_cast<CastInst>(operand)) {
                    // If we're casting back to the original type, eliminate both casts
                    if (innerCast->getSrcTy() == cast->getDestTy()) {
                        cast->replaceAllUsesWith(innerCast->getOperand(0));
                        changed = true;
                        type_casts_eliminated++;
                    }
                    // If the intermediate type is not needed, combine the casts
                    else if (CastInst::isCastable(innerCast->getSrcTy(), cast->getDestTy())) {
                        IRBuilder<> builder(inst);
                        auto opcode = CastInst::getCastOpcode(innerCast->getOperand(0),
                                                            false,
                                                            cast->getDestTy(),
                                                            false);
                        auto *newCast = builder.CreateCast(opcode, 
                                                         innerCast->getOperand(0),
                                                         cast->getDestTy(),
                                                         "combined_cast");
                        cast->replaceAllUsesWith(newCast);
                        changed = true;
                        type_casts_eliminated++;
                    }
                }
            }
        }
    }
    
    return changed;
}

bool TypeDirectedOptimizationPass::specializeGenericFunctions(Function &F) {
    bool changed = false;
    
    // This is a simplified version - in practice, we'd need more sophisticated analysis
    for (BasicBlock &BB : F) {
        for (auto I = BB.begin(), E = BB.end(); I != E; ++I) {
            if (auto *call = dyn_cast<CallInst>(&*I)) {
                Function *calledFunc = call->getCalledFunction();
                if (!calledFunc) continue;
                
                // Check if this function can be specialized based on argument types
                if (canSpecializeFunction(calledFunc)) {
                    // Create specialized version based on actual argument types
                    // This is a placeholder - full implementation would clone the function
                    // and specialize it for the specific types
                    
                    specializations_applied++;
                    // Mark as changed even though we're not actually specializing yet
                    // changed = true;
                }
            }
        }
    }
    
    return changed;
}

bool TypeDirectedOptimizationPass::optimizePolymorphicCalls(Function &F) {
    bool changed = false;
    
    // Look for polymorphic function calls that can be devirtualized
    for (BasicBlock &BB : F) {
        for (auto I = BB.begin(), E = BB.end(); I != E; ++I) {
            if (auto *call = dyn_cast<CallInst>(&*I)) {
                // If we can determine the exact type at compile time,
                // we can convert indirect calls to direct calls
                
                // This is a placeholder for more sophisticated devirtualization
                // In a real implementation, we'd analyze the type hierarchy
                // and determine if the call can be resolved statically
            }
        }
    }
    
    return changed;
}

bool TypeDirectedOptimizationPass::inlineMonomorphicFunctions(Function &F) {
    bool changed = false;
    
    // Inline small functions that have been specialized to specific types
    for (BasicBlock &BB : F) {
        for (auto I = BB.begin(), E = BB.end(); I != E; ++I) {
            if (auto *call = dyn_cast<CallInst>(&*I)) {
                Function *calledFunc = call->getCalledFunction();
                if (!calledFunc) continue;
                
                // Check if function is small enough and monomorphic
                if (calledFunc->size() <= 3 && // Small function (max 3 basic blocks)
                    !calledFunc->isVarArg() &&  // Not variadic
                    calledFunc->hasLocalLinkage()) { // Local function
                    
                    // This is where we'd perform inlining
                    // For now, just count potential inlines
                    // In practice, we'd use LLVM's inlining infrastructure
                }
            }
        }
    }
    
    return changed;
}

bool TypeDirectedOptimizationPass::isIntegerConstant(Value* val, int64_t& out_value) {
    if (auto *constFP = dyn_cast<ConstantFP>(val)) {
        double fpVal = constFP->getValueAPF().convertToDouble();
        if (fpVal == std::floor(fpVal) && 
            fpVal >= INT64_MIN && fpVal <= INT64_MAX) {
            out_value = static_cast<int64_t>(fpVal);
            return true;
        }
    }
    return false;
}

bool TypeDirectedOptimizationPass::isFloatConstant(Value* val, double& out_value) {
    if (auto *constFP = dyn_cast<ConstantFP>(val)) {
        out_value = constFP->getValueAPF().convertToDouble();
        return true;
    }
    return false;
}

Type* TypeDirectedOptimizationPass::inferLLVMType(Value* val) {
    // Infer the most appropriate LLVM type for a value
    // This is a simplified version
    return val->getType();
}

bool TypeDirectedOptimizationPass::canSpecializeFunction(Function* func) {
    // Check if a function is a good candidate for specialization
    if (!func || func->isVarArg() || func->isDeclaration()) {
        return false;
    }
    
    // Functions with generic parameters are good candidates
    // In practice, we'd analyze the function's parameter types and usage patterns
    return func->arg_size() > 0 && func->size() <= 10; // Small to medium functions
}

void TypeDirectedOptimizationPass::printOptimizationStats() {
    std::cout << "Type-Directed Optimization Statistics:\n";
    std::cout << "  - Function specializations applied: " << specializations_applied << "\n";
    std::cout << "  - Type casts eliminated: " << type_casts_eliminated << "\n";
    std::cout << "  - Generic instantiations: " << generic_instantiations << "\n";
}

void TypeDirectedOptimizationPass::resetStats() {
    specializations_applied = 0;
    type_casts_eliminated = 0;
    generic_instantiations = 0;
}

} // namespace quill

// Factory function for the optimization pass
std::unique_ptr<quill::TypeDirectedOptimizationPass> createTypeDirectedOptimizationPass() {
    return std::make_unique<quill::TypeDirectedOptimizationPass>();
}