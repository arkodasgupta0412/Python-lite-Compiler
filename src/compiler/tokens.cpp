#include "compiler/tokens.hpp"

namespace cd {

std::string tokenTypeName(TokenType type) {
  switch (type) {
    case TokenType::IDENT:
      return "IDENT";
    case TokenType::INT:
      return "INT";
    case TokenType::FLOAT:
      return "FLOAT";
    case TokenType::STRING:
      return "STRING";
    case TokenType::BOOL:
      return "BOOL";
    case TokenType::ASSIGN:
      return "ASSIGN";
    case TokenType::PLUS_ASSIGN:
      return "PLUS_ASSIGN";
    case TokenType::MINUS_ASSIGN:
      return "MINUS_ASSIGN";
    case TokenType::PLUS:
      return "PLUS";
    case TokenType::MINUS:
      return "MINUS";
    case TokenType::STAR:
      return "STAR";
    case TokenType::SLASH:
      return "SLASH";
    case TokenType::LPAREN:
      return "LPAREN";
    case TokenType::RPAREN:
      return "RPAREN";
    case TokenType::LBRACKET:
      return "LBRACKET";
    case TokenType::RBRACKET:
      return "RBRACKET";
    case TokenType::COMMA:
      return "COMMA";
    case TokenType::COLON:
      return "COLON";
    case TokenType::SEMICOLON:
      return "SEMICOLON";
    case TokenType::NEWLINE:
      return "NEWLINE";
    case TokenType::INDENT:
      return "INDENT";
    case TokenType::DEDENT:
      return "DEDENT";
    case TokenType::PRINT:
      return "PRINT";
    case TokenType::FOR:
      return "FOR";
    case TokenType::IN:
      return "IN";
    case TokenType::IF:
      return "IF";
    case TokenType::ELSE:
      return "ELSE";
    case TokenType::RANGE:
      return "RANGE";
    case TokenType::END_OF_FILE:
      return "EOF";
  }
  return "UNKNOWN";
}

}  // namespace cd
