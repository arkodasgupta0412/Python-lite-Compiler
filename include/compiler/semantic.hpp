#pragma once

#include <string>
#include <utility>
#include <vector>

#include "compiler/ast_nodes.hpp"
#include "compiler/symbol_table.hpp"

namespace cd {

class SemanticAnalyzer {
 public:
  explicit SemanticAnalyzer(SymbolTableManager& symbolTable) : symbolTable_(symbolTable) {}
  std::vector<std::string> analyze(const Program& program);

 private:
  SymbolTableManager& symbolTable_;
  std::vector<std::string> errors_;
  std::vector<std::vector<std::pair<const std::string*, const Type*>>> listElementTypeScopes_;

  void visitStatement(const Statement& stmt);
  void visitAssignment(const AssignmentStatement& stmt);
  void visitBlock(const BlockStatement& stmt);
  void visitFor(const ForStatement& stmt);
  void visitIf(const IfStatement& stmt);
  void enterScope();
  void exitScope();

  const Type* inferExprType(const Expression& expr);
  const Type* inferListElementType(const Expression& expr);
  const Type* resolveBinaryType(const Type* left, const std::string& op, const Type* right) const;

  const Type* trackedListElementType(const std::string* namePtr) const;
  void setTrackedListElementType(const std::string* namePtr, const Type* elemType);
  void eraseTrackedListElementType(const std::string* namePtr);
  static bool isErrorLike(const Type* type, const Type* poisonType);
};

}  // namespace cd
