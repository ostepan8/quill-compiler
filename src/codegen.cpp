#include "codegen.h"
#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/FileSystem.h>
#include <iostream>

CodeGen::CodeGen() {
    context = std::make_unique<llvm::LLVMContext>();
    module = std::make_unique<llvm::Module>("quill", *context);
    builder = std::make_unique<llvm::IRBuilder<>>(*context);
    current_function = nullptr;
}

void CodeGen::generate(ProgramAST& program) {
    program.codegen(*this);
}

llvm::Value* CodeGen::log_error_v(const char* str) {
    std::cerr << "Error: " << str << std::endl;
    return nullptr;
}

llvm::AllocaInst* CodeGen::create_entry_block_alloca(llvm::Function* function, const std::string& var_name) {
    llvm::IRBuilder<> tmp_builder(&function->getEntryBlock(), function->getEntryBlock().begin());
    return tmp_builder.CreateAlloca(llvm::Type::getDoubleTy(*context), 0, var_name.c_str());
}

llvm::Function* CodeGen::get_printf_function() {
    llvm::Function* printf_func = module->getFunction("printf");
    if (!printf_func) {
        llvm::FunctionType* printf_type = llvm::FunctionType::get(
            llvm::IntegerType::getInt32Ty(*context),
            llvm::PointerType::get(*context, 0),
            true);
        printf_func = llvm::Function::Create(printf_type,
            llvm::Function::ExternalLinkage, "printf", module.get());
    }
    return printf_func;
}

llvm::Function* CodeGen::get_print_double_function() {
    llvm::Function* print_func = module->getFunction("print_double");
    if (!print_func) {
        llvm::FunctionType* print_type = llvm::FunctionType::get(
            llvm::Type::getVoidTy(*context),
            {llvm::Type::getDoubleTy(*context)},
            false);
        print_func = llvm::Function::Create(print_type,
            llvm::Function::ExternalLinkage, "print_double", module.get());
    }
    return print_func;
}

void CodeGen::print_ir() {
    module->print(llvm::outs(), nullptr);
}

void CodeGen::write_object_file(const std::string& filename) {
    std::error_code ec;
    llvm::raw_fd_ostream dest(filename, ec, llvm::sys::fs::OF_None);

    if (ec) {
        std::cerr << "Could not open file: " << ec.message() << std::endl;
        return;
    }

    // For now, just write LLVM IR to the file
    module->print(dest, nullptr);
    dest.flush();
    
    std::cout << "Note: Generated LLVM IR instead of object file. " << std::endl;
    std::cout << "To compile to object: llc " << filename << " -o " << filename << ".o" << std::endl;
}