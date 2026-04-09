#pragma once

#include <string>

namespace cd {

enum class TokenType {
  IDENTIFIER,
  KEYWORD,
  NUMBER,
  FLOAT,
  STRING,
  BOOLEAN,
  OPERATOR,
  PUNCTUATOR,
  NEWLINE,
  INDENT,
  DEDENT,
  ENDMARKER
};

struct Token {
  TokenType type;
  std::string lexeme;
  int line;
  int column;
};

std::string tokenTypeName(TokenType type);
std::string tokenGrammarSymbol(const Token& token);

}  // namespace cd
