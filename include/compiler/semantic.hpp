#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "compiler/ast_nodes.hpp"
#include "compiler/symbol_table.hpp"

namespace cd {

class SemanticError : public std::runtime_error {
 public:
  explicit SemanticError(const std::string& msg) : std::runtime_error(msg) {}
};

class SemanticAnalyzer {
 public:
  explicit SemanticAnalyzer(ISymbolTable& symbolTable) : symbolTable_(symbolTable) {}
  void analyze(const Program& program);

 private:
  ISymbolTable& symbolTable_;
  std::vector<std::string> errors_;
  std::vector<std::unordered_map<std::string, std::string>> listElementTypeScopes_;

  void visitStatement(const Statement& stmt);
  void visitAssignment(const AssignmentStatement& stmt);
  void visitBlock(const BlockStatement& stmt);
  void visitFor(const ForStatement& stmt);
  void visitIf(const IfStatement& stmt);
  void enterScope();
  void exitScope();
  std::string inferExprType(const Expression& expr);
  std::string inferListElementType(const Expression& expr);
  static std::string resolveBinaryType(const std::string& left, const std::string& op, const std::string& right);
};

}  // namespace cd
