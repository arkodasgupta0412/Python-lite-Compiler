#include "compiler/tokens.hpp"

namespace cd {

std::string tokenTypeName(TokenType type) {
    switch (type) {
        case TokenType::IDENTIFIER:
            return "IDENTIFIER";
        case TokenType::KEYWORD:
            return "KEYWORD";
        case TokenType::NUMBER:
            return "NUMBER";
        case TokenType::FLOAT:
            return "FLOAT";
        case TokenType::STRING:
            return "STRING";
        case TokenType::BOOLEAN:
            return "BOOLEAN";
        case TokenType::OPERATOR:
            return "OPERATOR";
        case TokenType::PUNCTUATOR:
            return "PUNCTUATOR";
        case TokenType::NEWLINE:
            return "NEWLINE";
        case TokenType::INDENT:
            return "INDENT";
        case TokenType::DEDENT:
            return "DEDENT";
        case TokenType::ENDMARKER:
            return "EOF";
    }
    return "UNKNOWN";
}

std::string tokenGrammarSymbol(const Token& token) {
    switch (token.type) {
        case TokenType::IDENTIFIER:
            return "IDENT";
        case TokenType::NUMBER:
            return (token.lexeme.find('.') != std::string::npos) ? "FLOAT" : "INT";
        case TokenType::STRING:
            return "STRING";
        case TokenType::BOOLEAN:
            return "BOOL";
        case TokenType::NEWLINE:
            return "NEWLINE";
        case TokenType::INDENT:
            return "INDENT";
        case TokenType::DEDENT:
            return "DEDENT";
        case TokenType::ENDMARKER:
            return "EOF";
        case TokenType::KEYWORD:
            if (token.lexeme == "print") return "PRINT";
            if (token.lexeme == "for") return "FOR";
            if (token.lexeme == "in") return "IN";
            if (token.lexeme == "if") return "IF";
            if (token.lexeme == "else") return "ELSE";
            if (token.lexeme == "range") return "RANGE";
            return "KEYWORD";
        case TokenType::OPERATOR:
            if (token.lexeme == "=") return "ASSIGN";
            if (token.lexeme == "+=") return "PLUS_ASSIGN";
            if (token.lexeme == "-=") return "MINUS_ASSIGN";
            if (token.lexeme == "+") return "PLUS";
            if (token.lexeme == "-") return "MINUS";
            if (token.lexeme == "*") return "STAR";
            if (token.lexeme == "/") return "SLASH";
            return "OPERATOR";
        case TokenType::PUNCTUATOR:
            if (token.lexeme == "(") return "LPAREN";
            if (token.lexeme == ")") return "RPAREN";
            if (token.lexeme == "[") return "LBRACKET";
            if (token.lexeme == "]") return "RBRACKET";
            if (token.lexeme == ",") return "COMMA";
            if (token.lexeme == ":") return "COLON";
            if (token.lexeme == ";") return "SEMICOLON";
            return "PUNCTUATOR";
        case TokenType::FLOAT:
            return "FLOAT";
    }
    return "UNKNOWN";
}

}  // namespace cd
