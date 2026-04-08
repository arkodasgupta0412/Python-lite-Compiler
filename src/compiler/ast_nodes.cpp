#include "compiler/ast_nodes.hpp"

#include <sstream>

namespace cd {

std::string IntLiteral::repr() const { return "IntLiteral(" + std::to_string(value) + ")"; }
std::string FloatLiteral::repr() const { return "FloatLiteral(" + std::to_string(value) + ")"; }
std::string StringLiteral::repr() const { return "StringLiteral(\"" + value + "\")"; }
std::string BoolLiteral::repr() const { return std::string("BoolLiteral(") + (value ? "true" : "false") + ")"; }
std::string Identifier::repr() const { return "Identifier(" + name + ")"; }

UnaryExpression::UnaryExpression(std::string oper, std::unique_ptr<Expression> expr)
    : op(std::move(oper)), operand(std::move(expr)) {}

std::string UnaryExpression::repr() const { return "Unary(" + op + ", " + operand->repr() + ")"; }

BinaryExpression::BinaryExpression(std::unique_ptr<Expression> lhs, std::string oper,
                                   std::unique_ptr<Expression> rhs)
    : left(std::move(lhs)), op(std::move(oper)), right(std::move(rhs)) {}

std::string BinaryExpression::repr() const {
  return "Binary(" + left->repr() + " " + op + " " + right->repr() + ")";
}

std::string ListLiteral::repr() const {
  std::ostringstream out;
  out << "ListLiteral([";
  for (std::size_t i = 0; i < elements.size(); ++i) {
    out << elements[i]->repr();
    if (i + 1 < elements.size()) out << ", ";
  }
  out << "])";
  return out.str();
}

RangeExpression::RangeExpression(std::unique_ptr<Expression> startExpr, std::unique_ptr<Expression> endExpr,
                                 std::unique_ptr<Expression> stepExpr)
    : start(std::move(startExpr)), end(std::move(endExpr)), step(std::move(stepExpr)) {}

std::string RangeExpression::repr() const {
  std::ostringstream out;
  out << "Range(" << start->repr() << ", " << end->repr();
  if (step) out << ", " << step->repr();
  out << ")";
  return out.str();
}

AssignmentStatement::AssignmentStatement(std::string n, std::string oper, std::unique_ptr<Expression> expr)
    : name(std::move(n)), op(std::move(oper)), expression(std::move(expr)) {}

PrintStatement::PrintStatement(std::unique_ptr<Expression> expr) : expression(std::move(expr)) {}

ForStatement::ForStatement(std::string var, std::unique_ptr<Expression> iter, std::unique_ptr<Statement> stmt)
    : loopVar(std::move(var)), iterable(std::move(iter)), body(std::move(stmt)) {}

IfStatement::IfStatement(std::unique_ptr<Expression> conditionExpr, std::unique_ptr<Statement> thenStmt,
                         std::unique_ptr<Statement> elseStmt)
    : condition(std::move(conditionExpr)), thenBranch(std::move(thenStmt)), elseBranch(std::move(elseStmt)) {}

}  // namespace cd
