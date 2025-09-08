#include "../include/optimization_passes.h"
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Analysis/InlineCost.h>
#include <cmath>

using namespace llvm;
using namespace quill;

PreservedAnalyses QuillFunctionInliningPass::run(Module &M, ModuleAnalysisManager &AM) {
    bool changed = false;
    
    changed |= inlineSmallFunctions(M);
    
    if (changed) {
        return PreservedAnalyses::none();
    }
    
    return PreservedAnalyses::all();
}

bool QuillFunctionInliningPass::inlineSmallFunctions(Module &M) {
    bool changed = false;
    std::vector<CallInst*> callsToInline;
    
    // Find calls to small functions
    for (Function &caller : M) {
        if (caller.isDeclaration()) continue;
        
        for (BasicBlock &BB : caller) {
            for (Instruction &I : BB) {
                if (auto *call = dyn_cast<CallInst>(&I)) {
                    Function *callee = call->getCalledFunction();
                    if (callee && shouldInlineFunction(callee)) {
                        callsToInline.push_back(call);
                    }
                }
            }
        }
    }
    
    // Perform inlining
    for (CallInst *call : callsToInline) {
        Function *callee = call->getCalledFunction();
        
        // Skip recursive functions
        Function *caller = call->getParent()->getParent();
        if (caller == callee) continue;
        
        // Check for simple recursion in call chain
        bool isRecursive = false;
        for (BasicBlock &BB : *callee) {
            for (Instruction &I : BB) {
                if (auto *innerCall = dyn_cast<CallInst>(&I)) {
                    if (innerCall->getCalledFunction() == callee) {
                        isRecursive = true;
                        break;
                    }
                }
            }
            if (isRecursive) break;
        }
        
        if (isRecursive) continue;
        
        // Perform the inlining using LLVM's InlineFunction
        InlineFunctionInfo IFI;
        bool inlineSuccess = InlineFunction(*call, IFI).isSuccess();
        
        if (inlineSuccess) {
            changed = true;
        }
    }
    
    return changed;
}

bool QuillFunctionInliningPass::shouldInlineFunction(Function* func) {
    if (!func || func->isDeclaration()) {
        return false;
    }
    
    // Don't inline main function
    if (func->getName() == "main") {
        return false;
    }
    
    // Don't inline functions with complex control flow
    if (func->size() > 3) { // More than 3 basic blocks
        return false;
    }
    
    // Check instruction count
    int instCount = calculateInstructionCount(func);
    return instCount <= INLINE_THRESHOLD;
}

int QuillFunctionInliningPass::calculateInstructionCount(Function* func) {
    int count = 0;
    for (BasicBlock &BB : *func) {
        for (Instruction &I : BB) {
            count++;
            
            // Weight certain instructions more heavily
            if (isa<CallInst>(I)) {
                count += 5; // Function calls are expensive
            } else if (isa<BranchInst>(I)) {
                count += 1; // Branches are cheap
            } else if (isa<LoadInst>(I) || isa<StoreInst>(I)) {
                count += 2; // Memory operations
            }
        }
    }
    return count;
}