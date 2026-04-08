#include "compiler/semantic.hpp"

#include <sstream>

namespace cd {

void SemanticAnalyzer::enterScope() {
  symbolTable_.enterScope();
  listElementTypeScopes_.push_back({});
}

void SemanticAnalyzer::exitScope() {
  symbolTable_.exitScope();
  if (!listElementTypeScopes_.empty()) listElementTypeScopes_.pop_back();
}

void SemanticAnalyzer::analyze(const Program& program) {
  listElementTypeScopes_.clear();
  listElementTypeScopes_.push_back({});
  for (const auto& stmt : program.statements) visitStatement(*stmt);
  if (!errors_.empty()) {
    std::ostringstream out;
    for (std::size_t i = 0; i < errors_.size(); ++i) {
      out << errors_[i];
      if (i + 1 < errors_.size()) out << "\n";
    }
    throw SemanticError(out.str());
  }
}

void SemanticAnalyzer::visitStatement(const Statement& stmt) {
  if (auto p = dynamic_cast<const AssignmentStatement*>(&stmt)) {
    visitAssignment(*p);
    return;
  }
  if (auto p = dynamic_cast<const PrintStatement*>(&stmt)) {
    inferExprType(*p->expression);
    return;
  }
  if (auto p = dynamic_cast<const ForStatement*>(&stmt)) {
    visitFor(*p);
    return;
  }
  if (auto p = dynamic_cast<const IfStatement*>(&stmt)) {
    visitIf(*p);
    return;
  }
  if (auto p = dynamic_cast<const BlockStatement*>(&stmt)) {
    visitBlock(*p);
    return;
  }
  errors_.push_back("Unsupported statement");
}

void SemanticAnalyzer::visitAssignment(const AssignmentStatement& stmt) {
  auto rhsType = inferExprType(*stmt.expression);
  SymbolRecord existingRecord;
  bool hasExisting = symbolTable_.lookup(stmt.name, existingRecord);
  if (stmt.op == "=") {
    symbolTable_.assign(stmt.name, rhsType, stmt.expression->repr());
    auto elemType = inferListElementType(*stmt.expression);
    if (!elemType.empty() && !listElementTypeScopes_.empty()) {
      listElementTypeScopes_.back()[stmt.name] = elemType;
    }
    return;
  }

  if (!hasExisting) {
    errors_.push_back("Variable '" + stmt.name + "' must be initialized before '" + stmt.op + "'");
    return;
  }
  auto result = resolveBinaryType(existingRecord.varType, stmt.op.substr(0, 1), rhsType);
  if (result.empty()) {
    errors_.push_back("Type mismatch for '" + stmt.name + "'");
    return;
  }
  symbolTable_.assign(stmt.name, result, stmt.expression->repr());
}

void SemanticAnalyzer::visitFor(const ForStatement& stmt) {
  auto iterableType = inferExprType(*stmt.iterable);
  if (iterableType != "str" && iterableType != "list" && iterableType != "range") {
    errors_.push_back("For-loop iterable must be str, list, or range");
    return;
  }
  std::string elemType = "any";
  if (iterableType == "str") elemType = "str";
  if (iterableType == "list") elemType = inferListElementType(*stmt.iterable);
  if (iterableType == "range") elemType = "int";
  if (elemType.empty()) elemType = "any";
  enterScope();
  symbolTable_.assign(stmt.loopVar, elemType, "<loop-var>");
  if (!listElementTypeScopes_.empty()) listElementTypeScopes_.back().erase(stmt.loopVar);
  visitStatement(*stmt.body);
  exitScope();
}

void SemanticAnalyzer::visitBlock(const BlockStatement& stmt) {
  enterScope();
  for (const auto& bodyStmt : stmt.statements) {
    visitStatement(*bodyStmt);
  }
  exitScope();
}

void SemanticAnalyzer::visitIf(const IfStatement& stmt) {
  auto conditionType = inferExprType(*stmt.condition);
  if (conditionType != "bool") {
    errors_.push_back("if condition must be bool, found " + conditionType);
  }
  visitStatement(*stmt.thenBranch);
  if (stmt.elseBranch) visitStatement(*stmt.elseBranch);
}

std::string SemanticAnalyzer::inferExprType(const Expression& expr) {
  if (dynamic_cast<const IntLiteral*>(&expr)) return "int";
  if (dynamic_cast<const FloatLiteral*>(&expr)) return "float";
  if (dynamic_cast<const StringLiteral*>(&expr)) return "str";
  if (dynamic_cast<const BoolLiteral*>(&expr)) return "bool";
  if (dynamic_cast<const ListLiteral*>(&expr)) return "list";
  if (auto p = dynamic_cast<const RangeExpression*>(&expr)) {
    auto startType = inferExprType(*p->start);
    auto endType = inferExprType(*p->end);
    if (!(startType == "int" && endType == "int")) {
      errors_.push_back("range(...) start and end must be int");
      return "unknown";
    }
    if (p->step) {
      auto stepType = inferExprType(*p->step);
      if (stepType != "int") {
        errors_.push_back("range(...) step must be int");
        return "unknown";
      }
    }
    return "range";
  }

  if (auto p = dynamic_cast<const Identifier*>(&expr)) {
    SymbolRecord record;
    if (!symbolTable_.lookup(p->name, record)) {
      errors_.push_back("Undefined identifier '" + p->name + "'");
      return "unknown";
    }
    return record.varType;
  }

  if (auto p = dynamic_cast<const UnaryExpression*>(&expr)) {
    auto operandType = inferExprType(*p->operand);
    if (operandType == "int" || operandType == "float") return operandType;
    errors_.push_back("Unary operator expects numeric type");
    return "unknown";
  }

  if (auto p = dynamic_cast<const BinaryExpression*>(&expr)) {
    auto left = inferExprType(*p->left);
    auto right = inferExprType(*p->right);
    auto result = resolveBinaryType(left, p->op, right);
    if (result.empty()) {
      errors_.push_back("Operator '" + p->op + "' not valid for " + left + " and " + right);
      return "unknown";
    }
    return result;
  }

  errors_.push_back("Unsupported expression");
  return "unknown";
}

std::string SemanticAnalyzer::inferListElementType(const Expression& expr) {
  if (auto p = dynamic_cast<const ListLiteral*>(&expr)) {
    if (p->elements.empty()) return "any";
    auto first = inferExprType(*p->elements[0]);
    for (std::size_t i = 1; i < p->elements.size(); ++i) {
      if (inferExprType(*p->elements[i]) != first) return "any";
    }
    return first;
  }
  if (auto p = dynamic_cast<const Identifier*>(&expr)) {
    for (auto it = listElementTypeScopes_.rbegin(); it != listElementTypeScopes_.rend(); ++it) {
      auto found = it->find(p->name);
      if (found != it->end()) return found->second;
    }
  }
  return "";
}

std::string SemanticAnalyzer::resolveBinaryType(const std::string& left, const std::string& op,
                                                const std::string& right) {
  if (op == "+" || op == "-" || op == "--" || op == "*") {
    if ((left == "int" || left == "float") && (right == "int" || right == "float")) {
      return (left == "float" || right == "float") ? "float" : "int";
    }
    if (op == "+" && left == "str" && right == "str") return "str";
    if (op == "+" && left == "list" && right == "list") return "list";
    return "";
  }
  if (op == "/") {
    if ((left == "int" || left == "float") && (right == "int" || right == "float")) return "float";
    return "";
  }
  return "";
}

}  // namespace cd
