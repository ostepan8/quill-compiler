#pragma once
#include "token.h"
#include <vector>
#include <string>
#include <unordered_map>

class Lexer {
private:
    std::string source;
    size_t position;
    size_t line;
    size_t column;
    std::vector<size_t> indent_stack;
    std::unordered_map<std::string, TokenType> keywords;
    
    char current_char();
    char peek_char(size_t offset = 1);
    void advance();
    void skip_whitespace();
    void skip_comment();
    Token read_number();
    Token read_string();
    Token read_identifier();
    std::vector<Token> handle_indentation(const std::string& line);
    
public:
    Lexer(const std::string& src);
    std::vector<Token> tokenize();
};