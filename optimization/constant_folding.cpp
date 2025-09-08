#include "../include/optimization_passes.h"
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Dominators.h>
#include <llvm/Support/raw_ostream.h>
#include <iostream>
#include <cmath>

using namespace llvm;
using namespace quill;

PreservedAnalyses QuillConstantFoldingPass::run(Function &F, FunctionAnalysisManager &AM) {
    bool changed = false;
    
    // Fold binary operations with constant operands
    changed |= foldBinaryOperations(F);
    
    // Fold other constant expressions
    changed |= foldConstants(F);
    
    if (changed) {
        return PreservedAnalyses::none();
    }
    
    return PreservedAnalyses::all();
}

bool QuillConstantFoldingPass::foldBinaryOperations(Function &F) {
    bool changed = false;
    std::vector<Instruction*> toRemove;
    
    for (BasicBlock &BB : F) {
        for (Instruction &I : BB) {
            if (auto *binOp = dyn_cast<BinaryOperator>(&I)) {
                // Check if both operands are constants
                if (auto *result = evaluateConstantExpression(binOp)) {
                    // Replace all uses with the constant result
                    binOp->replaceAllUsesWith(result);
                    toRemove.push_back(binOp);
                    changed = true;
                }
            }
        }
    }
    
    // Remove folded instructions
    for (Instruction* inst : toRemove) {
        inst->eraseFromParent();
    }
    
    return changed;
}

bool QuillConstantFoldingPass::foldConstants(Function &F) {
    bool changed = false;
    
    // Look for patterns like: x = 5; y = x + 3; -> y = 8;
    for (BasicBlock &BB : F) {
        for (Instruction &I : BB) {
            if (auto *binOp = dyn_cast<BinaryOperator>(&I)) {
                Value *lhs = binOp->getOperand(0);
                Value *rhs = binOp->getOperand(1);
                
                // Try to trace back to constants through load instructions
                if (auto *loadLHS = dyn_cast<LoadInst>(lhs)) {
                    // Simple case: direct store-load pattern
                    if (auto *allocaLHS = dyn_cast<AllocaInst>(loadLHS->getPointerOperand())) {
                        // Find the most recent store to this alloca
                        for (User *user : allocaLHS->users()) {
                            if (auto *store = dyn_cast<StoreInst>(user)) {
                                if (auto *constant = dyn_cast<ConstantFP>(store->getValueOperand())) {
                                    // Replace the load with the constant
                                    loadLHS->replaceAllUsesWith(constant);
                                    changed = true;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    return changed;
}

Value* QuillConstantFoldingPass::evaluateConstantExpression(BinaryOperator* binOp) {
    ConstantFP *lhs = dyn_cast<ConstantFP>(binOp->getOperand(0));
    ConstantFP *rhs = dyn_cast<ConstantFP>(binOp->getOperand(1));
    
    if (!lhs || !rhs) return nullptr;
    
    double lval = lhs->getValueAPF().convertToDouble();
    double rval = rhs->getValueAPF().convertToDouble();
    double result = 0.0;
    
    switch (binOp->getOpcode()) {
        case Instruction::FAdd:
            result = lval + rval;
            break;
        case Instruction::FSub:
            result = lval - rval;
            break;
        case Instruction::FMul:
            result = lval * rval;
            break;
        case Instruction::FDiv:
            if (rval == 0.0) return nullptr; // Avoid division by zero
            result = lval / rval;
            break;
        case Instruction::FRem:
            if (rval == 0.0) return nullptr;
            result = fmod(lval, rval);
            break;
        default:
            return nullptr;
    }
    
    // Create new constant
    LLVMContext &ctx = binOp->getContext();
    return ConstantFP::get(ctx, APFloat(result));
}