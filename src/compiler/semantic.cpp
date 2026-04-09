#include "compiler/semantic.hpp"

namespace cd {

bool SemanticAnalyzer::isErrorLike(const Type* type, const Type* poisonType) {
  return type == nullptr || type == poisonType;
}

void SemanticAnalyzer::enterScope() {
  symbolTable_.enterScope();
  listElementTypeScopes_.push_back({});
}

void SemanticAnalyzer::exitScope() {
  symbolTable_.exitScope();
  if (!listElementTypeScopes_.empty()) listElementTypeScopes_.pop_back();
}

const Type* SemanticAnalyzer::trackedListElementType(const std::string* namePtr) const {
  for (auto it = listElementTypeScopes_.rbegin(); it != listElementTypeScopes_.rend(); ++it) {
    for (auto entry = it->rbegin(); entry != it->rend(); ++entry) {
      if (entry->first == namePtr) return entry->second;
    }
  }
  return nullptr;
}

void SemanticAnalyzer::setTrackedListElementType(const std::string* namePtr, const Type* elemType) {
  if (listElementTypeScopes_.empty()) return;
  auto& current = listElementTypeScopes_.back();
  for (auto& kv : current) {
    if (kv.first == namePtr) {
      kv.second = elemType;
      return;
    }
  }
  current.push_back({namePtr, elemType});
}

void SemanticAnalyzer::eraseTrackedListElementType(const std::string* namePtr) {
  if (listElementTypeScopes_.empty()) return;
  auto& current = listElementTypeScopes_.back();
  for (std::size_t i = 0; i < current.size(); ++i) {
    if (current[i].first == namePtr) {
      current.erase(current.begin() + static_cast<long long>(i));
      return;
    }
  }
}

std::vector<std::string> SemanticAnalyzer::analyze(const Program& program) {
  errors_.clear();
  symbolTable_.clearDiagnostics();

  listElementTypeScopes_.clear();
  listElementTypeScopes_.push_back({});

  for (const auto& stmt : program.statements) {
    visitStatement(*stmt);
  }

  for (const auto& diag : symbolTable_.diagnostics()) {
    errors_.push_back(diag);
  }

  return errors_;
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
  const Type* rhsType = inferExprType(*stmt.expression);
  const Type* poison = symbolTable_.poisonType();
  const Symbol* existing = symbolTable_.resolve(stmt.name, stmt.line, stmt.column, false);

  if (stmt.op == "=") {
    if (existing != nullptr) {
      if (existing->type != rhsType && !isErrorLike(existing->type, poison) && !isErrorLike(rhsType, poison)) {
        errors_.push_back("Type mismatch for assignment to '" + stmt.name + "': declared " +
                          typeDebugName(existing->type) + ", got " + typeDebugName(rhsType));
      }
      return;
    }

    symbolTable_.declare(stmt.name, rhsType, SymbolKind::Variable, stmt.line, stmt.column, -1);
    if (const Type* elem = inferListElementType(*stmt.expression)) {
      setTrackedListElementType(symbolTable_.internString(stmt.name), elem);
    }
    return;
  }

  if (existing == nullptr) {
    errors_.push_back("Variable '" + stmt.name + "' must be initialized before '" + stmt.op + "'");
    return;
  }

  const Type* resultType = resolveBinaryType(existing->type, stmt.op.substr(0, 1), rhsType);
  if (resultType == nullptr) {
    errors_.push_back("Type mismatch for assignment to '" + stmt.name + "'");
    return;
  }

  if (resultType != existing->type && !isErrorLike(resultType, poison) && !isErrorLike(existing->type, poison)) {
    errors_.push_back("Compound assignment changes declared type for '" + stmt.name + "' from " +
                      typeDebugName(existing->type) + " to " + typeDebugName(resultType));
  }
}

void SemanticAnalyzer::visitFor(const ForStatement& stmt) {
  const Type* iterableType = inferExprType(*stmt.iterable);
  const Type* strType = symbolTable_.strType();
  const Type* intType = symbolTable_.intType();
  const Type* poison = symbolTable_.poisonType();

  const Type* elementType = poison;
  bool iterableOk = false;

  if (iterableType == strType) {
    iterableOk = true;
    elementType = strType;
  } else if (iterableType != nullptr && iterableType->tag() == TypeTag::List) {
    iterableOk = true;
    elementType = static_cast<const ListType*>(iterableType)->elementType();
  }

  if (!iterableOk) {
    errors_.push_back("For-loop iterable must be str, list, or range");
    return;
  }

  enterScope();
  symbolTable_.declare(stmt.loopVar, elementType == nullptr ? intType : elementType, SymbolKind::Variable,
                       stmt.loopVarLine, stmt.loopVarColumn, -1);
  eraseTrackedListElementType(symbolTable_.internString(stmt.loopVar));
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
  const Type* conditionType = inferExprType(*stmt.condition);
  if (conditionType != symbolTable_.boolType() && conditionType != symbolTable_.poisonType()) {
    errors_.push_back("if condition must be bool, found " + typeDebugName(conditionType));
  }
  visitStatement(*stmt.thenBranch);
  if (stmt.elseBranch) visitStatement(*stmt.elseBranch);
}

const Type* SemanticAnalyzer::inferExprType(const Expression& expr) {
  const Type* intType = symbolTable_.intType();
  const Type* floatType = symbolTable_.floatType();
  const Type* strType = symbolTable_.strType();
  const Type* boolType = symbolTable_.boolType();
  const Type* poison = symbolTable_.poisonType();

  if (dynamic_cast<const IntLiteral*>(&expr)) return intType;
  if (dynamic_cast<const FloatLiteral*>(&expr)) return floatType;
  if (dynamic_cast<const StringLiteral*>(&expr)) return strType;
  if (dynamic_cast<const BoolLiteral*>(&expr)) return boolType;

  if (auto p = dynamic_cast<const ListLiteral*>(&expr)) {
    if (p->elements.empty()) {
      return symbolTable_.listType(poison);
    }

    const Type* elemType = inferExprType(*p->elements[0]);
    for (std::size_t i = 1; i < p->elements.size(); ++i) {
      const Type* cur = inferExprType(*p->elements[i]);
      if (cur != elemType && !isErrorLike(cur, poison) && !isErrorLike(elemType, poison)) {
        errors_.push_back("List literal has mixed element types");
        elemType = poison;
        break;
      }
    }
    return symbolTable_.listType(elemType == nullptr ? poison : elemType);
  }

  if (auto p = dynamic_cast<const RangeExpression*>(&expr)) {
    const Type* startType = inferExprType(*p->start);
    const Type* endType = inferExprType(*p->end);
    if (startType != intType || endType != intType) {
      errors_.push_back("range(...) start and end must be int");
      return poison;
    }
    if (p->step) {
      const Type* stepType = inferExprType(*p->step);
      if (stepType != intType) {
        errors_.push_back("range(...) step must be int");
        return poison;
      }
    }
    return symbolTable_.listType(intType);
  }

  if (auto p = dynamic_cast<const Identifier*>(&expr)) {
    const Symbol* symbol = symbolTable_.resolve(p->name, p->line, p->column);
    return symbol == nullptr ? poison : symbol->type;
  }

  if (auto p = dynamic_cast<const UnaryExpression*>(&expr)) {
    const Type* operandType = inferExprType(*p->operand);
    if (operandType == intType || operandType == floatType || operandType == poison) return operandType;
    errors_.push_back("Unary operator expects numeric type");
    return poison;
  }

  if (auto p = dynamic_cast<const BinaryExpression*>(&expr)) {
    const Type* leftType = inferExprType(*p->left);
    const Type* rightType = inferExprType(*p->right);
    const Type* result = resolveBinaryType(leftType, p->op, rightType);
    if (result == nullptr) {
      errors_.push_back("Operator '" + p->op + "' not valid for " + typeDebugName(leftType) + " and " +
                        typeDebugName(rightType));
      return poison;
    }
    return result;
  }

  errors_.push_back("Unsupported expression");
  return poison;
}

const Type* SemanticAnalyzer::inferListElementType(const Expression& expr) {
  const Type* poison = symbolTable_.poisonType();

  if (auto p = dynamic_cast<const ListLiteral*>(&expr)) {
    if (p->elements.empty()) return poison;
    const Type* first = inferExprType(*p->elements[0]);
    for (std::size_t i = 1; i < p->elements.size(); ++i) {
      if (inferExprType(*p->elements[i]) != first) return poison;
    }
    return first;
  }

  if (auto p = dynamic_cast<const RangeExpression*>(&expr)) {
    return symbolTable_.intType();
  }

  if (auto p = dynamic_cast<const Identifier*>(&expr)) {
    const std::string* namePtr = symbolTable_.internString(p->name);
    if (const Type* tracked = trackedListElementType(namePtr)) return tracked;

    const Symbol* symbol = symbolTable_.lookup(p->name);
    if (symbol != nullptr && symbol->type != nullptr && symbol->type->tag() == TypeTag::List) {
      return static_cast<const ListType*>(symbol->type)->elementType();
    }
  }

  if (const Type* t = inferExprType(expr); t != nullptr && t->tag() == TypeTag::List) {
    return static_cast<const ListType*>(t)->elementType();
  }
  return nullptr;
}

const Type* SemanticAnalyzer::resolveBinaryType(const Type* left, const std::string& op, const Type* right) const {
  const Type* intType = symbolTable_.intType();
  const Type* floatType = symbolTable_.floatType();
  const Type* strType = symbolTable_.strType();
  const Type* poison = symbolTable_.poisonType();

  if (isErrorLike(left, poison) || isErrorLike(right, poison)) return poison;

  const bool leftNumeric = (left == intType || left == floatType);
  const bool rightNumeric = (right == intType || right == floatType);

  if (op == "+" || op == "-" || op == "--" || op == "*") {
    if (leftNumeric && rightNumeric) {
      return (left == floatType || right == floatType) ? floatType : intType;
    }
    if (op == "+" && left == strType && right == strType) return strType;
    if (op == "+" && left->tag() == TypeTag::List && right->tag() == TypeTag::List) {
      const Type* leftElem = static_cast<const ListType*>(left)->elementType();
      const Type* rightElem = static_cast<const ListType*>(right)->elementType();
      return symbolTable_.listType(leftElem == rightElem ? leftElem : poison);
    }
    return nullptr;
  }

  if (op == "/") {
    if (leftNumeric && rightNumeric) return floatType;
    return nullptr;
  }

  return nullptr;
}

}  // namespace cd
