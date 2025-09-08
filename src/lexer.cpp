#include "lexer.h"
#include <cctype>
#include <stdexcept>

Lexer::Lexer(const std::string& src) : source(src), position(0), line(1), column(1) {
    indent_stack.push_back(0);
    
    keywords = {
        {"def", TokenType::DEF},
        {"if", TokenType::IF},
        {"else", TokenType::ELSE},
        {"elif", TokenType::ELIF},
        {"while", TokenType::WHILE},
        {"for", TokenType::FOR},
        {"return", TokenType::RETURN},
        {"print", TokenType::PRINT},
        {"True", TokenType::TRUE},
        {"False", TokenType::FALSE},
        {"and", TokenType::AND},
        {"or", TokenType::OR},
        {"not", TokenType::NOT},
    };
}

char Lexer::current_char() {
    if (position >= source.length()) return '\0';
    return source[position];
}

char Lexer::peek_char(size_t offset) {
    size_t peek_pos = position + offset;
    if (peek_pos >= source.length()) return '\0';
    return source[peek_pos];
}

void Lexer::advance() {
    if (position < source.length() && source[position] == '\n') {
        line++;
        column = 1;
    } else {
        column++;
    }
    position++;
}

void Lexer::skip_whitespace() {
    while (current_char() == ' ' || current_char() == '\t' || current_char() == '\r') {
        advance();
    }
}

void Lexer::skip_comment() {
    if (current_char() == '#') {
        while (current_char() != '\n' && current_char() != '\0') {
            advance();
        }
    }
}

Token Lexer::read_number() {
    std::string number;
    size_t start_line = line;
    size_t start_column = column;
    
    while (std::isdigit(current_char()) || current_char() == '.') {
        number += current_char();
        advance();
    }
    
    return Token(TokenType::NUMBER, number, start_line, start_column);
}

Token Lexer::read_string() {
    std::string str;
    size_t start_line = line;
    size_t start_column = column;
    char quote_char = current_char();
    advance(); // Skip opening quote
    
    while (current_char() != quote_char && current_char() != '\0') {
        if (current_char() == '\\') {
            advance();
            switch (current_char()) {
                case 'n': str += '\n'; break;
                case 't': str += '\t'; break;
                case 'r': str += '\r'; break;
                case '\\': str += '\\'; break;
                case '"': str += '"'; break;
                case '\'': str += '\''; break;
                default: str += current_char(); break;
            }
        } else {
            str += current_char();
        }
        advance();
    }
    
    if (current_char() == quote_char) {
        advance(); // Skip closing quote
    }
    
    return Token(TokenType::STRING, str, start_line, start_column);
}

Token Lexer::read_identifier() {
    std::string identifier;
    size_t start_line = line;
    size_t start_column = column;
    
    while (std::isalnum(current_char()) || current_char() == '_') {
        identifier += current_char();
        advance();
    }
    
    TokenType type = TokenType::IDENTIFIER;
    if (keywords.find(identifier) != keywords.end()) {
        type = keywords[identifier];
    }
    
    return Token(type, identifier, start_line, start_column);
}

std::vector<Token> Lexer::handle_indentation(const std::string& line) {
    std::vector<Token> tokens;
    size_t indent_level = 0;
    
    for (char c : line) {
        if (c == ' ') indent_level++;
        else if (c == '\t') indent_level += 4; // Treat tab as 4 spaces
        else break;
    }
    
    if (indent_level > indent_stack.back()) {
        indent_stack.push_back(indent_level);
        tokens.push_back(Token(TokenType::INDENT, "", this->line, 1));
    } else if (indent_level < indent_stack.back()) {
        while (indent_stack.size() > 1 && indent_level < indent_stack.back()) {
            indent_stack.pop_back();
            tokens.push_back(Token(TokenType::DEDENT, "", this->line, 1));
        }
    }
    
    return tokens;
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    
    while (current_char() != '\0') {
        skip_whitespace();
        skip_comment();
        
        if (current_char() == '\0') break;
        
        char c = current_char();
        size_t start_line = line;
        size_t start_column = column;
        
        // Handle newlines and indentation
        if (c == '\n') {
            tokens.push_back(Token(TokenType::NEWLINE, "\n", start_line, start_column));
            advance();
            
            // Check indentation of next line
            if (current_char() != '\0') {
                std::string next_line;
                size_t temp_pos = position;
                while (temp_pos < source.length() && source[temp_pos] != '\n') {
                    next_line += source[temp_pos++];
                }
                
                if (!next_line.empty() && next_line.find_first_not_of(" \t") != std::string::npos) {
                    auto indent_tokens = handle_indentation(next_line);
                    tokens.insert(tokens.end(), indent_tokens.begin(), indent_tokens.end());
                }
            }
            continue;
        }
        
        // Numbers
        if (std::isdigit(c)) {
            tokens.push_back(read_number());
            continue;
        }
        
        // Strings
        if (c == '"' || c == '\'') {
            tokens.push_back(read_string());
            continue;
        }
        
        // Identifiers and keywords
        if (std::isalpha(c) || c == '_') {
            tokens.push_back(read_identifier());
            continue;
        }
        
        // Two-character operators
        if (c == '=' && peek_char() == '=') {
            advance(); advance();
            tokens.push_back(Token(TokenType::EQUAL, "==", start_line, start_column));
            continue;
        }
        if (c == '!' && peek_char() == '=') {
            advance(); advance();
            tokens.push_back(Token(TokenType::NOT_EQUAL, "!=", start_line, start_column));
            continue;
        }
        if (c == '<' && peek_char() == '=') {
            advance(); advance();
            tokens.push_back(Token(TokenType::LESS_EQUAL, "<=", start_line, start_column));
            continue;
        }
        if (c == '>' && peek_char() == '=') {
            advance(); advance();
            tokens.push_back(Token(TokenType::GREATER_EQUAL, ">=", start_line, start_column));
            continue;
        }
        
        // Single-character tokens
        advance();
        switch (c) {
            case '+': tokens.push_back(Token(TokenType::PLUS, "+", start_line, start_column)); break;
            case '-': tokens.push_back(Token(TokenType::MINUS, "-", start_line, start_column)); break;
            case '*': tokens.push_back(Token(TokenType::MULTIPLY, "*", start_line, start_column)); break;
            case '/': tokens.push_back(Token(TokenType::DIVIDE, "/", start_line, start_column)); break;
            case '%': tokens.push_back(Token(TokenType::MODULO, "%", start_line, start_column)); break;
            case '=': tokens.push_back(Token(TokenType::ASSIGN, "=", start_line, start_column)); break;
            case '<': tokens.push_back(Token(TokenType::LESS_THAN, "<", start_line, start_column)); break;
            case '>': tokens.push_back(Token(TokenType::GREATER_THAN, ">", start_line, start_column)); break;
            case '(': tokens.push_back(Token(TokenType::LEFT_PAREN, "(", start_line, start_column)); break;
            case ')': tokens.push_back(Token(TokenType::RIGHT_PAREN, ")", start_line, start_column)); break;
            case '[': tokens.push_back(Token(TokenType::LEFT_BRACKET, "[", start_line, start_column)); break;
            case ']': tokens.push_back(Token(TokenType::RIGHT_BRACKET, "]", start_line, start_column)); break;
            case ',': tokens.push_back(Token(TokenType::COMMA, ",", start_line, start_column)); break;
            case ':': tokens.push_back(Token(TokenType::COLON, ":", start_line, start_column)); break;
            default:
                throw std::runtime_error("Unexpected character: " + std::string(1, c));
        }
    }
    
    // Add final DEDENT tokens
    while (indent_stack.size() > 1) {
        indent_stack.pop_back();
        tokens.push_back(Token(TokenType::DEDENT, "", line, column));
    }
    
    tokens.push_back(Token(TokenType::EOF_TOKEN, "", line, column));
    return tokens;
}