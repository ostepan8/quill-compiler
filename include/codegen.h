#pragma once
#include "ast.h"
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Value.h>
#include <unordered_map>
#include <memory>

class CodeGen {
public:
    std::unique_ptr<llvm::LLVMContext> context;
    std::unique_ptr<llvm::Module> module;
    std::unique_ptr<llvm::IRBuilder<>> builder;
    
    std::unordered_map<std::string, llvm::AllocaInst*> named_values;
    llvm::Function* current_function;
    
    CodeGen();
    
    void generate(ProgramAST& program);
    llvm::Value* log_error_v(const char* str);
    llvm::AllocaInst* create_entry_block_alloca(llvm::Function* function, const std::string& var_name);
    
    void print_ir();
    void write_object_file(const std::string& filename);
    
    // Helper functions for builtin operations
    llvm::Function* get_printf_function();
    llvm::Function* get_print_double_function();
};