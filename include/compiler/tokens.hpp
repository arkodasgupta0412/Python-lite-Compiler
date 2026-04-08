#pragma once

#include <string>

namespace cd {

enum class TokenType {
  IDENT,
  INT,
  FLOAT,
  STRING,
  BOOL,
  ASSIGN,
  PLUS_ASSIGN,
  MINUS_ASSIGN,
  PLUS,
  MINUS,
  STAR,
  SLASH,
  LPAREN,
  RPAREN,
  LBRACKET,
  RBRACKET,
  COMMA,
  COLON,
  SEMICOLON,
  NEWLINE,
  INDENT,
  DEDENT,
  PRINT,
  FOR,
  IN,
  IF,
  ELSE,
  RANGE,
  END_OF_FILE
};

struct Token {
  TokenType type;
  std::string lexeme;
  int line;
  int column;
};

std::string tokenTypeName(TokenType type);

}  // namespace cd
