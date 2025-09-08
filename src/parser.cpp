#include "parser.h"
#include <stdexcept>

Parser::Parser(std::vector<Token> toks) : tokens(std::move(toks)), current(0) {}

Token& Parser::current_token() {
    if (current >= tokens.size()) {
        static Token eof(TokenType::EOF_TOKEN, "", 0, 0);
        return eof;
    }
    return tokens[current];
}

Token& Parser::peek_token(size_t offset) {
    size_t pos = current + offset;
    if (pos >= tokens.size()) {
        static Token eof(TokenType::EOF_TOKEN, "", 0, 0);
        return eof;
    }
    return tokens[pos];
}

void Parser::advance() {
    if (current < tokens.size()) current++;
}

bool Parser::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

bool Parser::check(TokenType type) {
    return current_token().type == type;
}

void Parser::consume(TokenType type, const std::string& message) {
    if (!match(type)) {
        throw std::runtime_error(message + " at line " + std::to_string(current_token().line));
    }
}

void Parser::skip_newlines() {
    while (match(TokenType::NEWLINE)) {}
}

std::unique_ptr<ExprAST> Parser::parse_primary() {
    if (match(TokenType::NUMBER)) {
        double value = std::stod(tokens[current - 1].value);
        return std::make_unique<NumberExprAST>(value);
    }
    
    if (match(TokenType::STRING)) {
        std::string value = tokens[current - 1].value;
        return std::make_unique<StringExprAST>(value);
    }
    
    if (match(TokenType::IDENTIFIER)) {
        std::string name = tokens[current - 1].value;
        
        // Function call
        if (match(TokenType::LEFT_PAREN)) {
            std::vector<std::unique_ptr<ExprAST>> args;
            
            if (!check(TokenType::RIGHT_PAREN)) {
                do {
                    args.push_back(parse_expression());
                } while (match(TokenType::COMMA));
            }
            
            consume(TokenType::RIGHT_PAREN, "Expected ')' after function arguments");
            return std::make_unique<CallExprAST>(name, std::move(args));
        }
        
        // Variable
        return std::make_unique<VariableExprAST>(name);
    }
    
    if (match(TokenType::LEFT_PAREN)) {
        auto expr = parse_expression();
        consume(TokenType::RIGHT_PAREN, "Expected ')' after expression");
        return expr;
    }
    
    if (match(TokenType::TRUE)) {
        return std::make_unique<NumberExprAST>(1.0);
    }
    
    if (match(TokenType::FALSE)) {
        return std::make_unique<NumberExprAST>(0.0);
    }
    
    throw std::runtime_error("Expected expression at line " + std::to_string(current_token().line));
}

std::unique_ptr<ExprAST> Parser::parse_unary() {
    if (match(TokenType::MINUS) || match(TokenType::NOT)) {
        char op = tokens[current - 1].value[0];
        auto operand = parse_unary();
        return std::make_unique<UnaryExprAST>(op, std::move(operand));
    }
    
    return parse_primary();
}

std::unique_ptr<ExprAST> Parser::parse_factor() {
    auto expr = parse_unary();
    
    while (match(TokenType::MULTIPLY) || match(TokenType::DIVIDE) || match(TokenType::MODULO)) {
        char op = tokens[current - 1].value[0];
        auto right = parse_unary();
        expr = std::make_unique<BinaryExprAST>(op, std::move(expr), std::move(right));
    }
    
    return expr;
}

std::unique_ptr<ExprAST> Parser::parse_term() {
    auto expr = parse_factor();
    
    while (match(TokenType::PLUS) || match(TokenType::MINUS)) {
        char op = tokens[current - 1].value[0];
        auto right = parse_factor();
        expr = std::make_unique<BinaryExprAST>(op, std::move(expr), std::move(right));
    }
    
    return expr;
}

std::unique_ptr<ExprAST> Parser::parse_comparison() {
    auto expr = parse_term();
    
    while (match(TokenType::LESS_THAN) || match(TokenType::LESS_EQUAL) ||
           match(TokenType::GREATER_THAN) || match(TokenType::GREATER_EQUAL)) {
        // Use a unique char for each comparison operator
        char op;
        TokenType token_type = tokens[current - 1].type;
        if (token_type == TokenType::LESS_THAN) op = '<';
        else if (token_type == TokenType::LESS_EQUAL) op = 'L';  // L for <=
        else if (token_type == TokenType::GREATER_THAN) op = '>';
        else op = 'G';  // G for >=
        
        auto right = parse_term();
        expr = std::make_unique<BinaryExprAST>(op, std::move(expr), std::move(right));
    }
    
    return expr;
}

std::unique_ptr<ExprAST> Parser::parse_equality() {
    auto expr = parse_comparison();
    
    while (match(TokenType::EQUAL) || match(TokenType::NOT_EQUAL)) {
        char op = (tokens[current - 1].type == TokenType::EQUAL) ? '=' : '!';
        auto right = parse_comparison();
        expr = std::make_unique<BinaryExprAST>(op, std::move(expr), std::move(right));
    }
    
    return expr;
}

std::unique_ptr<ExprAST> Parser::parse_logical_and() {
    auto expr = parse_equality();
    
    while (match(TokenType::AND)) {
        char op = '&';
        auto right = parse_equality();
        expr = std::make_unique<BinaryExprAST>(op, std::move(expr), std::move(right));
    }
    
    return expr;
}

std::unique_ptr<ExprAST> Parser::parse_logical_or() {
    auto expr = parse_logical_and();
    
    while (match(TokenType::OR)) {
        char op = '|';
        auto right = parse_logical_and();
        expr = std::make_unique<BinaryExprAST>(op, std::move(expr), std::move(right));
    }
    
    return expr;
}

std::unique_ptr<ExprAST> Parser::parse_expression() {
    return parse_logical_or();
}

std::unique_ptr<StmtAST> Parser::parse_assignment() {
    if (check(TokenType::IDENTIFIER) && peek_token().type == TokenType::ASSIGN) {
        std::string name = current_token().value;
        advance(); // identifier
        advance(); // =
        
        auto value = parse_expression();
        return std::make_unique<AssignmentStmtAST>(name, std::move(value));
    }
    
    return parse_expression_statement();
}

std::unique_ptr<StmtAST> Parser::parse_expression_statement() {
    auto expr = parse_expression();
    return std::make_unique<ExprStmtAST>(std::move(expr));
}

std::unique_ptr<StmtAST> Parser::parse_if_statement() {
    consume(TokenType::IF, "Expected 'if'");
    auto condition = parse_expression();
    consume(TokenType::COLON, "Expected ':' after if condition");
    skip_newlines();
    
    auto then_stmt = parse_block();
    std::unique_ptr<StmtAST> else_stmt = nullptr;
    
    if (match(TokenType::ELSE)) {
        consume(TokenType::COLON, "Expected ':' after 'else'");
        skip_newlines();
        else_stmt = parse_block();
    }
    
    return std::make_unique<IfStmtAST>(std::move(condition), std::move(then_stmt), std::move(else_stmt));
}

std::unique_ptr<StmtAST> Parser::parse_while_statement() {
    consume(TokenType::WHILE, "Expected 'while'");
    auto condition = parse_expression();
    consume(TokenType::COLON, "Expected ':' after while condition");
    skip_newlines();
    
    auto body = parse_block();
    return std::make_unique<WhileStmtAST>(std::move(condition), std::move(body));
}

std::unique_ptr<StmtAST> Parser::parse_return_statement() {
    consume(TokenType::RETURN, "Expected 'return'");
    
    std::unique_ptr<ExprAST> value = nullptr;
    if (!check(TokenType::NEWLINE) && !check(TokenType::EOF_TOKEN)) {
        value = parse_expression();
    }
    
    return std::make_unique<ReturnStmtAST>(std::move(value));
}

std::unique_ptr<StmtAST> Parser::parse_print_statement() {
    consume(TokenType::PRINT, "Expected 'print'");
    consume(TokenType::LEFT_PAREN, "Expected '(' after 'print'");
    
    auto expr = parse_expression();
    
    consume(TokenType::RIGHT_PAREN, "Expected ')' after print expression");
    return std::make_unique<PrintStmtAST>(std::move(expr));
}

std::unique_ptr<StmtAST> Parser::parse_block() {
    consume(TokenType::INDENT, "Expected indented block");
    
    std::vector<std::unique_ptr<StmtAST>> statements;
    
    while (!check(TokenType::DEDENT) && !check(TokenType::EOF_TOKEN)) {
        skip_newlines();
        if (check(TokenType::DEDENT) || check(TokenType::EOF_TOKEN)) break;
        
        statements.push_back(parse_statement());
        skip_newlines();
    }
    
    consume(TokenType::DEDENT, "Expected dedent to end block");
    return std::make_unique<BlockStmtAST>(std::move(statements));
}

std::unique_ptr<StmtAST> Parser::parse_statement() {
    if (check(TokenType::IF)) {
        return parse_if_statement();
    }
    
    if (check(TokenType::WHILE)) {
        return parse_while_statement();
    }
    
    if (check(TokenType::RETURN)) {
        return parse_return_statement();
    }
    
    if (check(TokenType::PRINT)) {
        return parse_print_statement();
    }
    
    return parse_assignment();
}

std::unique_ptr<FunctionAST> Parser::parse_function() {
    consume(TokenType::DEF, "Expected 'def'");
    
    if (!check(TokenType::IDENTIFIER)) {
        throw std::runtime_error("Expected function name");
    }
    
    std::string name = current_token().value;
    advance();
    
    consume(TokenType::LEFT_PAREN, "Expected '(' after function name");
    
    std::vector<std::string> args;
    if (!check(TokenType::RIGHT_PAREN)) {
        do {
            if (!check(TokenType::IDENTIFIER)) {
                throw std::runtime_error("Expected parameter name");
            }
            args.push_back(current_token().value);
            advance();
        } while (match(TokenType::COMMA));
    }
    
    consume(TokenType::RIGHT_PAREN, "Expected ')' after parameters");
    consume(TokenType::COLON, "Expected ':' after function signature");
    skip_newlines();
    
    auto body = parse_block();
    return std::make_unique<FunctionAST>(name, std::move(args), std::move(body));
}

std::unique_ptr<ProgramAST> Parser::parse() {
    std::vector<std::unique_ptr<FunctionAST>> functions;
    
    skip_newlines();
    
    while (!check(TokenType::EOF_TOKEN)) {
        functions.push_back(parse_function());
        skip_newlines();
    }
    
    return std::make_unique<ProgramAST>(std::move(functions));
}