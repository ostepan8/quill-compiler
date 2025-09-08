#include "../include/optimization_passes.h"
#include <llvm/IR/PatternMatch.h>

using namespace llvm;
using namespace llvm::PatternMatch;
using namespace quill;

PreservedAnalyses QuillArithmeticSimplificationPass::run(Function &F, FunctionAnalysisManager &AM) {
    bool changed = false;
    
    changed |= simplifyArithmetic(F);
    
    if (changed) {
        return PreservedAnalyses::none();
    }
    
    return PreservedAnalyses::all();
}

bool QuillArithmeticSimplificationPass::simplifyArithmetic(Function &F) {
    bool changed = false;
    std::vector<Instruction*> toRemove;
    
    for (BasicBlock &BB : F) {
        for (Instruction &I : BB) {
            if (auto *binOp = dyn_cast<BinaryOperator>(&I)) {
                if (auto *simplified = simplifyExpression(binOp)) {
                    binOp->replaceAllUsesWith(simplified);
                    toRemove.push_back(binOp);
                    changed = true;
                }
            }
        }
    }
    
    // Remove simplified instructions
    for (Instruction* inst : toRemove) {
        inst->eraseFromParent();
    }
    
    return changed;
}

Value* QuillArithmeticSimplificationPass::simplifyExpression(BinaryOperator* binOp) {
    Value *lhs = binOp->getOperand(0);
    Value *rhs = binOp->getOperand(1);
    LLVMContext &ctx = binOp->getContext();
    
    switch (binOp->getOpcode()) {
        case Instruction::FAdd:
            // X + 0 = X
            if (isZero(rhs)) return lhs;
            if (isZero(lhs)) return rhs;
            
            // X + X = 2 * X (could be beneficial)
            if (lhs == rhs) {
                // Create 2.0 * X
                Value *two = ConstantFP::get(Type::getDoubleTy(ctx), 2.0);
                return BinaryOperator::CreateFMul(lhs, two, "double", binOp->getIterator());
            }
            break;
            
        case Instruction::FSub:
            // X - 0 = X
            if (isZero(rhs)) return lhs;
            
            // X - X = 0
            if (lhs == rhs) {
                return ConstantFP::get(Type::getDoubleTy(ctx), 0.0);
            }
            break;
            
        case Instruction::FMul:
            // X * 0 = 0
            if (isZero(lhs) || isZero(rhs)) {
                return ConstantFP::get(Type::getDoubleTy(ctx), 0.0);
            }
            
            // X * 1 = X
            if (isOne(rhs)) return lhs;
            if (isOne(lhs)) return rhs;
            
            // X * 2 can be replaced with X + X (sometimes faster)
            if (auto *constant = dyn_cast<ConstantFP>(rhs)) {
                if (constant->getValueAPF().convertToDouble() == 2.0) {
                    return BinaryOperator::CreateFAdd(lhs, lhs, "double", binOp->getIterator());
                }
            }
            if (auto *constant = dyn_cast<ConstantFP>(lhs)) {
                if (constant->getValueAPF().convertToDouble() == 2.0) {
                    return BinaryOperator::CreateFAdd(rhs, rhs, "double", binOp->getIterator());
                }
            }
            break;
            
        case Instruction::FDiv:
            // X / 1 = X
            if (isOne(rhs)) return lhs;
            
            // X / X = 1 (assuming X != 0)
            if (lhs == rhs) {
                return ConstantFP::get(Type::getDoubleTy(ctx), 1.0);
            }
            
            // 0 / X = 0 (assuming X != 0)
            if (isZero(lhs)) {
                return ConstantFP::get(Type::getDoubleTy(ctx), 0.0);
            }
            break;
            
        default:
            break;
    }
    
    return nullptr; // No simplification found
}

bool QuillArithmeticSimplificationPass::isZero(Value* val) {
    if (auto *constant = dyn_cast<ConstantFP>(val)) {
        return constant->getValueAPF().convertToDouble() == 0.0;
    }
    return false;
}

bool QuillArithmeticSimplificationPass::isOne(Value* val) {
    if (auto *constant = dyn_cast<ConstantFP>(val)) {
        return constant->getValueAPF().convertToDouble() == 1.0;
    }
    return false;
}