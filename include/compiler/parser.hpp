#pragma once

#include <stdexcept>
#include <vector>

#include "compiler/ast_nodes.hpp"
#include "compiler/tokens.hpp"

namespace cd {

class ParseError : public std::runtime_error {
 public:
  explicit ParseError(const std::string& msg) : std::runtime_error(msg) {}
};

class TopDownParser {
 public:
  explicit TopDownParser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}
  Program parse();

 private:
  std::vector<Token> tokens_;
  std::size_t current_{0};

  std::unique_ptr<Statement> statement();
  std::unique_ptr<Statement> printStatement();
  std::unique_ptr<Statement> forStatement();
  std::unique_ptr<Statement> ifStatement();
  std::unique_ptr<Statement> assignmentStatement();
  std::unique_ptr<Statement> blockStatement();

  std::unique_ptr<Expression> expression();
  std::unique_ptr<Expression> term();
  std::unique_ptr<Expression> factor();
  std::unique_ptr<Expression> primary();

  void consumeSeparators();
  bool match(std::initializer_list<TokenType> types);
  const Token& consume(TokenType type, const std::string& message);
  bool check(TokenType type) const;
  const Token& advance();
  bool isAtEnd() const;
  const Token& peek() const;
  const Token& previous() const;
};

}  // namespace cd
