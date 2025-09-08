#pragma once
#include "lexer.h"
#include "ast.h"
#include <memory>

class Parser {
private:
    std::vector<Token> tokens;
    size_t current;
    
    Token& current_token();
    Token& peek_token(size_t offset = 1);
    void advance();
    bool match(TokenType type);
    bool check(TokenType type);
    void consume(TokenType type, const std::string& message);
    
    std::unique_ptr<ExprAST> parse_primary();
    std::unique_ptr<ExprAST> parse_unary();
    std::unique_ptr<ExprAST> parse_factor();
    std::unique_ptr<ExprAST> parse_term();
    std::unique_ptr<ExprAST> parse_comparison();
    std::unique_ptr<ExprAST> parse_equality();
    std::unique_ptr<ExprAST> parse_logical_and();
    std::unique_ptr<ExprAST> parse_logical_or();
    std::unique_ptr<ExprAST> parse_expression();
    
    std::unique_ptr<StmtAST> parse_assignment();
    std::unique_ptr<StmtAST> parse_expression_statement();
    std::unique_ptr<StmtAST> parse_if_statement();
    std::unique_ptr<StmtAST> parse_while_statement();
    std::unique_ptr<StmtAST> parse_return_statement();
    std::unique_ptr<StmtAST> parse_print_statement();
    std::unique_ptr<StmtAST> parse_block();
    std::unique_ptr<StmtAST> parse_statement();
    
    std::unique_ptr<FunctionAST> parse_function();
    
    void skip_newlines();
    
public:
    Parser(std::vector<Token> toks);
    std::unique_ptr<ProgramAST> parse();
};