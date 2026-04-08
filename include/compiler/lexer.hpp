#pragma once

#include <stdexcept>
#include <string>
#include <vector>

#include "compiler/tokens.hpp"

namespace cd {

class LexicalError : public std::runtime_error {
 public:
  explicit LexicalError(const std::string& msg) : std::runtime_error(msg) {}
};

class Lexer {
 public:
  explicit Lexer(std::string source) : source_(std::move(source)) {}
  std::vector<Token> tokenize() const;

 private:
  std::string source_;
};

}  // namespace cd
