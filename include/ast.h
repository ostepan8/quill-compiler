#pragma once
#include <memory>
#include <vector>
#include <string>
// #include "type_system.h"  // Temporarily disabled

namespace llvm {
    class Value;
}

namespace quill {
    class Type;  // Forward declaration
}

class CodeGen;

class ASTNode {
public:
    // std::unique_ptr<quill::Type> inferred_type;  // Temporarily disabled
    
    virtual ~ASTNode() = default;
    virtual llvm::Value* codegen(CodeGen& gen) = 0;
    
    // Type information accessors (temporarily disabled)
    // quill::Type* getInferredType() const { return inferred_type.get(); }
    // void setInferredType(std::unique_ptr<quill::Type> type) { inferred_type = std::move(type); }
    // bool hasTypeInformation() const { return inferred_type != nullptr; }
};

class ExprAST : public ASTNode {
public:
    virtual ~ExprAST() = default;
};

class NumberExprAST : public ExprAST {
public:
    double value;
    
    NumberExprAST(double val) : value(val) {}
    llvm::Value* codegen(CodeGen& gen) override;
};

class StringExprAST : public ExprAST {
public:
    std::string value;
    
    StringExprAST(const std::string& val) : value(val) {}
    llvm::Value* codegen(CodeGen& gen) override;
};

class VariableExprAST : public ExprAST {
public:
    std::string name;
    
    VariableExprAST(const std::string& n) : name(n) {}
    llvm::Value* codegen(CodeGen& gen) override;
};

class BinaryExprAST : public ExprAST {
public:
    char op;
    std::unique_ptr<ExprAST> lhs, rhs;
    
    BinaryExprAST(char o, std::unique_ptr<ExprAST> l, std::unique_ptr<ExprAST> r)
        : op(o), lhs(std::move(l)), rhs(std::move(r)) {}
    llvm::Value* codegen(CodeGen& gen) override;
};

class UnaryExprAST : public ExprAST {
public:
    char op;
    std::unique_ptr<ExprAST> operand;
    
    UnaryExprAST(char o, std::unique_ptr<ExprAST> operand)
        : op(o), operand(std::move(operand)) {}
    llvm::Value* codegen(CodeGen& gen) override;
};

class CallExprAST : public ExprAST {
public:
    std::string callee;
    std::vector<std::unique_ptr<ExprAST>> args;
    
    CallExprAST(const std::string& c, std::vector<std::unique_ptr<ExprAST>> a)
        : callee(c), args(std::move(a)) {}
    llvm::Value* codegen(CodeGen& gen) override;
};

class StmtAST : public ASTNode {
public:
    virtual ~StmtAST() = default;
};

class AssignmentStmtAST : public StmtAST {
public:
    std::string name;
    std::unique_ptr<ExprAST> value;
    
    AssignmentStmtAST(const std::string& n, std::unique_ptr<ExprAST> v)
        : name(n), value(std::move(v)) {}
    llvm::Value* codegen(CodeGen& gen) override;
};

class ExprStmtAST : public StmtAST {
public:
    std::unique_ptr<ExprAST> expression;
    
    ExprStmtAST(std::unique_ptr<ExprAST> expr) : expression(std::move(expr)) {}
    llvm::Value* codegen(CodeGen& gen) override;
};

class BlockStmtAST : public StmtAST {
public:
    std::vector<std::unique_ptr<StmtAST>> statements;
    
    BlockStmtAST(std::vector<std::unique_ptr<StmtAST>> stmts)
        : statements(std::move(stmts)) {}
    llvm::Value* codegen(CodeGen& gen) override;
};

class IfStmtAST : public StmtAST {
public:
    std::unique_ptr<ExprAST> condition;
    std::unique_ptr<StmtAST> then_stmt;
    std::unique_ptr<StmtAST> else_stmt;
    
    IfStmtAST(std::unique_ptr<ExprAST> cond, std::unique_ptr<StmtAST> then_s,
              std::unique_ptr<StmtAST> else_s = nullptr)
        : condition(std::move(cond)), then_stmt(std::move(then_s)), 
          else_stmt(std::move(else_s)) {}
    llvm::Value* codegen(CodeGen& gen) override;
};

class WhileStmtAST : public StmtAST {
public:
    std::unique_ptr<ExprAST> condition;
    std::unique_ptr<StmtAST> body;
    
    WhileStmtAST(std::unique_ptr<ExprAST> cond, std::unique_ptr<StmtAST> b)
        : condition(std::move(cond)), body(std::move(b)) {}
    llvm::Value* codegen(CodeGen& gen) override;
};

class ReturnStmtAST : public StmtAST {
public:
    std::unique_ptr<ExprAST> value;
    
    ReturnStmtAST(std::unique_ptr<ExprAST> val = nullptr) : value(std::move(val)) {}
    llvm::Value* codegen(CodeGen& gen) override;
};

class PrintStmtAST : public StmtAST {
public:
    std::unique_ptr<ExprAST> expression;
    
    PrintStmtAST(std::unique_ptr<ExprAST> expr) : expression(std::move(expr)) {}
    llvm::Value* codegen(CodeGen& gen) override;
};

class FunctionAST : public ASTNode {
public:
    std::string name;
    std::vector<std::string> args;
    std::unique_ptr<StmtAST> body;
    
    FunctionAST(const std::string& n, std::vector<std::string> a, 
                std::unique_ptr<StmtAST> b)
        : name(n), args(std::move(a)), body(std::move(b)) {}
    llvm::Value* codegen(CodeGen& gen) override;
};

class ProgramAST : public ASTNode {
public:
    std::vector<std::unique_ptr<FunctionAST>> functions;
    
    ProgramAST(std::vector<std::unique_ptr<FunctionAST>> funcs)
        : functions(std::move(funcs)) {}
    llvm::Value* codegen(CodeGen& gen) override;
};