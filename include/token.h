#pragma once
#include <string>

enum class TokenType {
    EOF_TOKEN,
    
    // Literals
    NUMBER,
    STRING,
    IDENTIFIER,
    
    // Keywords
    DEF,
    IF,
    ELSE,
    ELIF,
    WHILE,
    FOR,
    RETURN,
    PRINT,
    TRUE,
    FALSE,
    
    // Operators
    PLUS,
    MINUS,
    MULTIPLY,
    DIVIDE,
    MODULO,
    ASSIGN,
    
    // Comparison
    EQUAL,
    NOT_EQUAL,
    LESS_THAN,
    LESS_EQUAL,
    GREATER_THAN,
    GREATER_EQUAL,
    
    // Logical
    AND,
    OR,
    NOT,
    
    // Punctuation
    LEFT_PAREN,
    RIGHT_PAREN,
    LEFT_BRACKET,
    RIGHT_BRACKET,
    COMMA,
    COLON,
    
    // Special
    NEWLINE,
    INDENT,
    DEDENT,
};

struct Token {
    TokenType type;
    std::string value;
    size_t line;
    size_t column;
    
    Token(TokenType t, const std::string& v, size_t l, size_t c)
        : type(t), value(v), line(l), column(c) {}
};