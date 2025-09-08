#include "../include/optimization_passes.h"
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Support/Casting.h>
#include <iostream>

using namespace llvm;
using namespace quill;

QuillTypeDirectedOptimizationPass::QuillTypeDirectedOptimizationPass() {
    resetStats();
}

PreservedAnalyses QuillTypeDirectedOptimizationPass::run(Function &F, FunctionAnalysisManager &AM) {
    bool changed = false;
    
    // Run various type-directed optimizations
    changed |= optimizeNumericOperations(F);
    changed |= eliminateUnnecessaryTypeCasts(F);
    changed |= specializeGenericFunctions(F);
    changed |= optimizePolymorphicCalls(F);
    changed |= inlineMonomorphicFunctions(F);
    
    return changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
}

bool QuillTypeDirectedOptimizationPass::optimizeNumericOperations(Function &F) {
    bool changed = false;
    std::vector<Instruction*> toErase;
    
    for (BasicBlock &BB : F) {
        for (Instruction &I : BB) {
            // Optimize integer operations that don't need floating point
            if (auto *binOp = dyn_cast<BinaryOperator>(&I)) {
                Value *left = binOp->getOperand(0);
                Value *right = binOp->getOperand(1);
                
                // Check for integer constants being treated as floats
                int64_t leftInt, rightInt;
                if (isIntegerConstant(left, leftInt) && isIntegerConstant(right, rightInt)) {
                    
                    switch (binOp->getOpcode()) {
                        case Instruction::FAdd: {
                            // Replace floating point add with integer add if both operands are integers
                            IRBuilder<> builder(binOp);
                            auto *leftConst = ConstantInt::get(builder.getInt64Ty(), leftInt);
                            auto *rightConst = ConstantInt::get(builder.getInt64Ty(), rightInt);
                            auto *intAdd = builder.CreateAdd(leftConst, rightConst, "int_add_opt");
                            
                            // Convert back to float for compatibility
                            auto *floatResult = builder.CreateSIToFP(intAdd, builder.getDoubleTy());
                            
                            binOp->replaceAllUsesWith(floatResult);
                            toErase.push_back(binOp);
                            changed = true;
                            stats.numeric_optimizations++;
                            stats.integer_arithmetic_optimized++;
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
                                
                                IRBuilder<> builder(binOp);
                                auto *leftConst = ConstantInt::get(builder.getInt64Ty(), leftInt);
                                auto *shiftConst = ConstantInt::get(builder.getInt32Ty(), shift_amount);
                                auto *shifted = builder.CreateShl(leftConst, shiftConst, "shift_opt");
                                auto *floatResult = builder.CreateSIToFP(shifted, builder.getDoubleTy());
                                
                                binOp->replaceAllUsesWith(floatResult);
                                toErase.push_back(binOp);
                                changed = true;
                                stats.numeric_optimizations++;
                                stats.multiplication_to_shifts++;
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
                                
                                IRBuilder<> builder(binOp);
                                auto *leftConst = ConstantInt::get(builder.getInt64Ty(), leftInt);
                                auto *shiftConst = ConstantInt::get(builder.getInt32Ty(), shift_amount);
                                auto *shifted = builder.CreateAShr(leftConst, shiftConst, "div_shift_opt");
                                auto *floatResult = builder.CreateSIToFP(shifted, builder.getDoubleTy());
                                
                                binOp->replaceAllUsesWith(floatResult);
                                toErase.push_back(binOp);
                                changed = true;
                                stats.numeric_optimizations++;
                                stats.divisions_to_shifts++;
                            }
                            break;
                        }
                        
                        default:
                            break;
                    }
                }
            }
            
            // Optimize comparisons
            else if (auto *cmp = dyn_cast<FCmpInst>(&I)) {
                Value *left = cmp->getOperand(0);
                Value *right = cmp->getOperand(1);
                
                int64_t leftInt, rightInt;
                if (isIntegerConstant(left, leftInt) && isIntegerConstant(right, rightInt)) {
                    // Replace floating point comparison with integer comparison
                    IRBuilder<> builder(cmp);
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
                    toErase.push_back(cmp);
                    changed = true;
                    stats.numeric_optimizations++;
                    stats.integer_arithmetic_optimized++;
                }
            }
        }
    }
    
    // Erase replaced instructions
    for (auto *inst : toErase) {
        inst->eraseFromParent();
    }
    
    return changed;
}

bool QuillTypeDirectedOptimizationPass::eliminateUnnecessaryTypeCasts(Function &F) {
    bool changed = false;
    std::vector<Instruction*> toErase;
    
    for (BasicBlock &BB : F) {
        for (Instruction &I : BB) {
            // Look for unnecessary cast chains
            if (auto *cast = dyn_cast<CastInst>(&I)) {
                Value *operand = cast->getOperand(0);
                
                // Check for cast-to-same-type
                if (cast->getSrcTy() == cast->getDestTy()) {
                    cast->replaceAllUsesWith(operand);
                    toErase.push_back(cast);
                    changed = true;
                    stats.type_casts_eliminated++;
                    continue;
                }
                
                // Check for cast chains (cast(cast(x)))
                if (auto *innerCast = dyn_cast<CastInst>(operand)) {
                    // If we're casting back to the original type, eliminate both casts
                    if (innerCast->getSrcTy() == cast->getDestTy()) {
                        cast->replaceAllUsesWith(innerCast->getOperand(0));
                        toErase.push_back(cast);
                        changed = true;
                        stats.type_casts_eliminated++;
                    }
                    // If the intermediate type is not needed, combine the casts  
                    else {
                        auto opcode = CastInst::getCastOpcode(innerCast->getOperand(0), false,
                                                            cast->getDestTy(), false);
                        if (CastInst::castIsValid(opcode, innerCast->getSrcTy(), cast->getDestTy())) {
                            IRBuilder<> builder(cast);
                            auto *newCast = builder.CreateCast(opcode, 
                                                             innerCast->getOperand(0),
                                                             cast->getDestTy(),
                                                             "combined_cast");
                            cast->replaceAllUsesWith(newCast);
                            toErase.push_back(cast);
                            changed = true;
                            stats.type_casts_eliminated++;
                        }
                    }
                }
            }
        }
    }
    
    // Erase replaced instructions
    for (auto *inst : toErase) {
        inst->eraseFromParent();
    }
    
    return changed;
}

bool QuillTypeDirectedOptimizationPass::specializeGenericFunctions(Function &F) {
    bool changed = false;
    
    for (BasicBlock &BB : F) {
        for (Instruction &I : BB) {
            if (auto *call = dyn_cast<CallInst>(&I)) {
                Function *calledFunc = call->getCalledFunction();
                if (!calledFunc) continue;
                
                if (canSpecializeFunction(calledFunc)) {
                    stats.specializations_applied++;
                }
            }
        }
    }
    
    return changed;
}

bool QuillTypeDirectedOptimizationPass::optimizePolymorphicCalls(Function &F) {
    bool changed = false;
    
    for (BasicBlock &BB : F) {
        for (Instruction &I : BB) {
            if (auto *call = dyn_cast<CallInst>(&I)) {
                // Devirtualization logic would go here
            }
        }
    }
    
    return changed;
}

bool QuillTypeDirectedOptimizationPass::inlineMonomorphicFunctions(Function &F) {
    bool changed = false;
    
    for (BasicBlock &BB : F) {
        for (Instruction &I : BB) {
            if (auto *call = dyn_cast<CallInst>(&I)) {
                Function *calledFunc = call->getCalledFunction();
                if (!calledFunc) continue;
                
                if (calledFunc->size() <= 3 && 
                    !calledFunc->isVarArg() && 
                    calledFunc->hasLocalLinkage()) {
                    // Inlining logic would go here
                }
            }
        }
    }
    
    return changed;
}

bool QuillTypeDirectedOptimizationPass::isIntegerConstant(Value* val, int64_t& out_value) {
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

bool QuillTypeDirectedOptimizationPass::isFloatConstant(Value* val, double& out_value) {
    if (auto *constFP = dyn_cast<ConstantFP>(val)) {
        out_value = constFP->getValueAPF().convertToDouble();
        return true;
    }
    return false;
}

bool QuillTypeDirectedOptimizationPass::canSpecializeFunction(Function* func) {
    if (!func || func->isVarArg() || func->isDeclaration()) {
        return false;
    }
    
    return func->arg_size() > 0 && func->size() <= 10;
}

bool QuillTypeDirectedOptimizationPass::isPowerOfTwo(int64_t value) {
    return value > 0 && (value & (value - 1)) == 0;
}

int QuillTypeDirectedOptimizationPass::getShiftAmount(int64_t power_of_two) {
    int shift_amount = 0;
    int64_t temp = power_of_two;
    while (temp > 1) {
        temp >>= 1;
        shift_amount++;
    }
    return shift_amount;
}

bool QuillTypeDirectedOptimizationPass::optimizeIntegerArithmetic(Function &F) {
    return false;
}

bool QuillTypeDirectedOptimizationPass::optimizeDivisionToShifts(Function &F) {
    return false;
}

bool QuillTypeDirectedOptimizationPass::optimizeMultiplicationToShifts(Function &F) {
    return false;
}