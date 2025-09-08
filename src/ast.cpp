#include "ast.h"
#include "codegen.h"
#include <llvm/IR/Value.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Verifier.h>
#include <iostream>

llvm::Value* NumberExprAST::codegen(CodeGen& gen) {
    return llvm::ConstantFP::get(*gen.context, llvm::APFloat(value));
}

llvm::Value* StringExprAST::codegen(CodeGen& gen) {
    return gen.builder->CreateGlobalString(value);
}

llvm::Value* VariableExprAST::codegen(CodeGen& gen) {
    llvm::AllocaInst* alloca = gen.named_values[name];
    if (!alloca) {
        return gen.log_error_v(("Unknown variable name: " + name).c_str());
    }
    
    return gen.builder->CreateLoad(alloca->getAllocatedType(), alloca, name.c_str());
}

llvm::Value* BinaryExprAST::codegen(CodeGen& gen) {
    llvm::Value* l = lhs->codegen(gen);
    llvm::Value* r = rhs->codegen(gen);
    if (!l || !r) return nullptr;
    
    switch (op) {
        case '+':
            return gen.builder->CreateFAdd(l, r, "addtmp");
        case '-':
            return gen.builder->CreateFSub(l, r, "subtmp");
        case '*':
            return gen.builder->CreateFMul(l, r, "multmp");
        case '/':
            return gen.builder->CreateFDiv(l, r, "divtmp");
        case '%':
            return gen.builder->CreateFRem(l, r, "remtmp");
        case '<':
            l = gen.builder->CreateFCmpULT(l, r, "cmptmp");
            return gen.builder->CreateUIToFP(l, llvm::Type::getDoubleTy(*gen.context), "booltmp");
        case 'L': // <=
            l = gen.builder->CreateFCmpULE(l, r, "cmptmp");
            return gen.builder->CreateUIToFP(l, llvm::Type::getDoubleTy(*gen.context), "booltmp");
        case '>':
            l = gen.builder->CreateFCmpUGT(l, r, "cmptmp");
            return gen.builder->CreateUIToFP(l, llvm::Type::getDoubleTy(*gen.context), "booltmp");
        case 'G': // >=
            l = gen.builder->CreateFCmpUGE(l, r, "cmptmp");
            return gen.builder->CreateUIToFP(l, llvm::Type::getDoubleTy(*gen.context), "booltmp");
        case '=': // ==
            l = gen.builder->CreateFCmpUEQ(l, r, "cmptmp");
            return gen.builder->CreateUIToFP(l, llvm::Type::getDoubleTy(*gen.context), "booltmp");
        case '!': // !=
            l = gen.builder->CreateFCmpUNE(l, r, "cmptmp");
            return gen.builder->CreateUIToFP(l, llvm::Type::getDoubleTy(*gen.context), "booltmp");
        case '&': // and
            l = gen.builder->CreateFCmpONE(l, llvm::ConstantFP::get(*gen.context, llvm::APFloat(0.0)), "lhs");
            r = gen.builder->CreateFCmpONE(r, llvm::ConstantFP::get(*gen.context, llvm::APFloat(0.0)), "rhs");
            l = gen.builder->CreateAnd(l, r, "andtmp");
            return gen.builder->CreateUIToFP(l, llvm::Type::getDoubleTy(*gen.context), "booltmp");
        case '|': // or
            l = gen.builder->CreateFCmpONE(l, llvm::ConstantFP::get(*gen.context, llvm::APFloat(0.0)), "lhs");
            r = gen.builder->CreateFCmpONE(r, llvm::ConstantFP::get(*gen.context, llvm::APFloat(0.0)), "rhs");
            l = gen.builder->CreateOr(l, r, "ortmp");
            return gen.builder->CreateUIToFP(l, llvm::Type::getDoubleTy(*gen.context), "booltmp");
        default:
            return gen.log_error_v("invalid binary operator");
    }
}

llvm::Value* UnaryExprAST::codegen(CodeGen& gen) {
    llvm::Value* operand_val = operand->codegen(gen);
    if (!operand_val) return nullptr;
    
    switch (op) {
        case '-':
            return gen.builder->CreateFNeg(operand_val, "negtmp");
        case '!': // not
            operand_val = gen.builder->CreateFCmpOEQ(operand_val, 
                llvm::ConstantFP::get(*gen.context, llvm::APFloat(0.0)), "nottmp");
            return gen.builder->CreateUIToFP(operand_val, llvm::Type::getDoubleTy(*gen.context), "booltmp");
        default:
            return gen.log_error_v("invalid unary operator");
    }
}

llvm::Value* CallExprAST::codegen(CodeGen& gen) {
    llvm::Function* callee_func = gen.module->getFunction(callee);
    if (!callee_func) {
        return gen.log_error_v(("Unknown function referenced: " + callee).c_str());
    }
    
    if (callee_func->arg_size() != args.size()) {
        return gen.log_error_v("Incorrect number of arguments passed");
    }
    
    std::vector<llvm::Value*> args_v;
    for (auto& arg : args) {
        args_v.push_back(arg->codegen(gen));
        if (!args_v.back()) return nullptr;
    }
    
    return gen.builder->CreateCall(callee_func, args_v, "calltmp");
}

llvm::Value* AssignmentStmtAST::codegen(CodeGen& gen) {
    llvm::Value* val = value->codegen(gen);
    if (!val) return nullptr;
    
    llvm::AllocaInst* alloca = gen.named_values[name];
    if (!alloca) {
        // Create new variable
        alloca = gen.create_entry_block_alloca(gen.current_function, name);
        gen.named_values[name] = alloca;
    }
    
    gen.builder->CreateStore(val, alloca);
    return val;
}

llvm::Value* ExprStmtAST::codegen(CodeGen& gen) {
    return expression->codegen(gen);
}

llvm::Value* BlockStmtAST::codegen(CodeGen& gen) {
    llvm::Value* last_val = nullptr;
    for (auto& stmt : statements) {
        last_val = stmt->codegen(gen);
        if (!last_val) return nullptr;
    }
    return last_val ? last_val : llvm::Constant::getNullValue(llvm::Type::getDoubleTy(*gen.context));
}

llvm::Value* IfStmtAST::codegen(CodeGen& gen) {
    llvm::Value* cond_val = condition->codegen(gen);
    if (!cond_val) return nullptr;
    
    // Convert condition to a bool by comparing non-equal to 0.0
    cond_val = gen.builder->CreateFCmpONE(
        cond_val, llvm::ConstantFP::get(*gen.context, llvm::APFloat(0.0)), "ifcond");
    
    llvm::Function* function = gen.builder->GetInsertBlock()->getParent();
    
    // Create blocks for the then and else cases. Insert the 'then' block at the
    // end of the function.
    llvm::BasicBlock* then_bb = llvm::BasicBlock::Create(*gen.context, "then", function);
    llvm::BasicBlock* else_bb = llvm::BasicBlock::Create(*gen.context, "else");
    llvm::BasicBlock* merge_bb = llvm::BasicBlock::Create(*gen.context, "ifcont");
    
    gen.builder->CreateCondBr(cond_val, then_bb, else_bb);
    
    // Emit then value.
    gen.builder->SetInsertPoint(then_bb);
    llvm::Value* then_val = then_stmt->codegen(gen);
    if (!then_val) return nullptr;
    
    // Only create branch if block doesn't already have terminator
    bool then_has_terminator = gen.builder->GetInsertBlock()->getTerminator() != nullptr;
    if (!then_has_terminator) {
        gen.builder->CreateBr(merge_bb);
    }
    // Codegen of 'then' can change the current block, update then_bb for the PHI.
    then_bb = gen.builder->GetInsertBlock();
    
    // Emit else block.
    else_bb->insertInto(function);
    gen.builder->SetInsertPoint(else_bb);
    
    llvm::Value* else_val = nullptr;
    if (else_stmt) {
        else_val = else_stmt->codegen(gen);
        if (!else_val) return nullptr;
    } else {
        else_val = llvm::Constant::getNullValue(llvm::Type::getDoubleTy(*gen.context));
    }
    
    // Only create branch if block doesn't already have terminator
    bool else_has_terminator = gen.builder->GetInsertBlock()->getTerminator() != nullptr;
    if (!else_has_terminator) {
        gen.builder->CreateBr(merge_bb);
    }
    // Codegen of 'else' can change the current block, update else_bb for the PHI.
    else_bb = gen.builder->GetInsertBlock();
    
    // Emit merge block.
    merge_bb->insertInto(function);
    gen.builder->SetInsertPoint(merge_bb);
    
    // Only create PHI if both branches flow to the merge block
    if (then_val && else_val && 
        !then_has_terminator && !else_has_terminator &&
        then_val->getType() == else_val->getType()) {
        llvm::PHINode* phi_node = gen.builder->CreatePHI(then_val->getType(), 2, "iftmp");
        phi_node->addIncoming(then_val, then_bb);
        phi_node->addIncoming(else_val, else_bb);
        return phi_node;
    }
    
    // Return a default value if branches don't match or have early returns
    return llvm::Constant::getNullValue(llvm::Type::getDoubleTy(*gen.context));
}

llvm::Value* WhileStmtAST::codegen(CodeGen& gen) {
    llvm::Function* function = gen.builder->GetInsertBlock()->getParent();
    
    llvm::BasicBlock* loop_bb = llvm::BasicBlock::Create(*gen.context, "loop", function);
    llvm::BasicBlock* after_bb = llvm::BasicBlock::Create(*gen.context, "afterloop", function);
    
    // Fall into the loop.
    gen.builder->CreateBr(loop_bb);
    gen.builder->SetInsertPoint(loop_bb);
    
    // Emit the body of the loop.
    if (!body->codegen(gen)) return nullptr;
    
    // Emit the step value.
    llvm::Value* cond_val = condition->codegen(gen);
    if (!cond_val) return nullptr;
    
    // Convert condition to a bool by comparing non-equal to 0.0.
    cond_val = gen.builder->CreateFCmpONE(
        cond_val, llvm::ConstantFP::get(*gen.context, llvm::APFloat(0.0)), "loopcond");
    
    gen.builder->CreateCondBr(cond_val, loop_bb, after_bb);
    gen.builder->SetInsertPoint(after_bb);
    
    // for expr always returns 0.0.
    return llvm::Constant::getNullValue(llvm::Type::getDoubleTy(*gen.context));
}

llvm::Value* ReturnStmtAST::codegen(CodeGen& gen) {
    llvm::Value* ret_val = nullptr;
    if (value) {
        ret_val = value->codegen(gen);
        if (!ret_val) return nullptr;
    } else {
        ret_val = llvm::Constant::getNullValue(llvm::Type::getDoubleTy(*gen.context));
    }
    
    gen.builder->CreateRet(ret_val);
    return ret_val;
}

llvm::Value* PrintStmtAST::codegen(CodeGen& gen) {
    llvm::Value* val = expression->codegen(gen);
    if (!val) return nullptr;
    
    // For now, just print numbers. In a full implementation, you'd handle strings too.
    llvm::Function* print_func = gen.get_print_double_function();
    gen.builder->CreateCall(print_func, val);
    
    // Return the value that was printed
    return val;
}

llvm::Value* FunctionAST::codegen(CodeGen& gen) {
    // Create function type
    std::vector<llvm::Type*> doubles(args.size(), llvm::Type::getDoubleTy(*gen.context));
    llvm::FunctionType* ft = llvm::FunctionType::get(llvm::Type::getDoubleTy(*gen.context), doubles, false);
    
    llvm::Function* function = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, name, gen.module.get());
    
    // Set argument names
    unsigned idx = 0;
    for (auto& arg : function->args()) {
        arg.setName(args[idx++]);
    }
    
    // Create a new basic block to start insertion into.
    llvm::BasicBlock* bb = llvm::BasicBlock::Create(*gen.context, "entry", function);
    gen.builder->SetInsertPoint(bb);
    
    // Record the function arguments in the NamedValues map.
    gen.named_values.clear();
    gen.current_function = function;
    
    for (auto& arg : function->args()) {
        // Create an alloca for this variable.
        llvm::AllocaInst* alloca = gen.create_entry_block_alloca(function, std::string(arg.getName()));
        
        // Store the initial value into the alloca.
        gen.builder->CreateStore(&arg, alloca);
        
        // Add arguments to variable symbol table.
        gen.named_values[std::string(arg.getName())] = alloca;
    }
    
    if (llvm::Value* ret_val = body->codegen(gen)) {
        // Only add return if the current block doesn't already have a terminator
        if (!gen.builder->GetInsertBlock()->getTerminator()) {
            gen.builder->CreateRet(ret_val);
        }
        
        // Validate the generated code, checking for consistency.
        verifyFunction(*function);
        
        return function;
    }
    
    // Error reading body, remove function.
    function->eraseFromParent();
    return nullptr;
}

llvm::Value* ProgramAST::codegen(CodeGen& gen) {
    for (auto& func : functions) {
        func->codegen(gen);
    }
    return nullptr;
}