#include "../include/optimization_passes.h"
#include <llvm/IR/CFG.h>
#include <llvm/Transforms/Utils/Local.h>
#include <set>

using namespace llvm;
using namespace quill;

PreservedAnalyses QuillDeadCodeEliminationPass::run(Function &F, FunctionAnalysisManager &AM) {
    bool changed = false;
    
    // Eliminate dead instructions
    changed |= eliminateDeadInstructions(F);
    
    // Eliminate unreachable basic blocks
    changed |= eliminateUnreachableBlocks(F);
    
    if (changed) {
        return PreservedAnalyses::none();
    }
    
    return PreservedAnalyses::all();
}

bool QuillDeadCodeEliminationPass::eliminateDeadInstructions(Function &F) {
    bool changed = false;
    std::vector<Instruction*> toRemove;
    
    // Multiple passes to catch cascading dead code
    bool madeChange = true;
    while (madeChange) {
        madeChange = false;
        toRemove.clear();
        
        for (BasicBlock &BB : F) {
            for (Instruction &I : BB) {
                if (isInstructionDead(&I)) {
                    toRemove.push_back(&I);
                    madeChange = true;
                    changed = true;
                }
            }
        }
        
        // Remove dead instructions
        for (Instruction* inst : toRemove) {
            inst->eraseFromParent();
        }
    }
    
    return changed;
}

bool QuillDeadCodeEliminationPass::eliminateUnreachableBlocks(Function &F) {
    bool changed = false;
    std::set<BasicBlock*> reachable;
    std::vector<BasicBlock*> worklist;
    
    // Start from entry block
    BasicBlock* entry = &F.getEntryBlock();
    reachable.insert(entry);
    worklist.push_back(entry);
    
    // Mark all reachable blocks
    while (!worklist.empty()) {
        BasicBlock* current = worklist.back();
        worklist.pop_back();
        
        for (BasicBlock* succ : successors(current)) {
            if (reachable.find(succ) == reachable.end()) {
                reachable.insert(succ);
                worklist.push_back(succ);
            }
        }
    }
    
    // Remove unreachable blocks
    std::vector<BasicBlock*> toRemove;
    for (BasicBlock &BB : F) {
        if (reachable.find(&BB) == reachable.end()) {
            toRemove.push_back(&BB);
            changed = true;
        }
    }
    
    for (BasicBlock* bb : toRemove) {
        bb->eraseFromParent();
    }
    
    return changed;
}

bool QuillDeadCodeEliminationPass::isInstructionDead(Instruction* inst) {
    // Don't remove instructions with side effects
    if (inst->mayHaveSideEffects()) {
        return false;
    }
    
    // Don't remove terminators
    if (inst->isTerminator()) {
        return false;
    }
    
    // Don't remove instructions that are used
    if (!inst->use_empty()) {
        return false;
    }
    
    // Special cases for specific instruction types
    if (isa<StoreInst>(inst)) {
        // Store instructions have side effects
        return false;
    }
    
    if (isa<CallInst>(inst)) {
        // Function calls may have side effects
        return false;
    }
    
    // Instruction is dead if it has no uses and no side effects
    return true;
}