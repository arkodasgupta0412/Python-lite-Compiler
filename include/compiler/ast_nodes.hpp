#pragma once

#include <memory>
#include <string>
#include <vector>

namespace cd {

struct Node {
  virtual ~Node() = default;
};

struct Expression : public Node {
  virtual std::string repr() const = 0;
};

struct Statement : public Node {};

struct IntLiteral final : public Expression {
  int value{};
  explicit IntLiteral(int v) : value(v) {}
  std::string repr() const override;
};

struct FloatLiteral final : public Expression {
  double value{};
  explicit FloatLiteral(double v) : value(v) {}
  std::string repr() const override;
};

struct StringLiteral final : public Expression {
  std::string value;
  explicit StringLiteral(std::string v) : value(std::move(v)) {}
  std::string repr() const override;
};

struct BoolLiteral final : public Expression {
  bool value{};
  explicit BoolLiteral(bool v) : value(v) {}
  std::string repr() const override;
};

struct Identifier final : public Expression {
  std::string name;
  int line{0};
  int column{0};
  explicit Identifier(std::string n, int lineNo = 0, int columnNo = 0)
      : name(std::move(n)), line(lineNo), column(columnNo) {}
  std::string repr() const override;
};

struct UnaryExpression final : public Expression {
  std::string op;
  std::unique_ptr<Expression> operand;
  UnaryExpression(std::string oper, std::unique_ptr<Expression> expr);
  std::string repr() const override;
};

struct BinaryExpression final : public Expression {
  std::unique_ptr<Expression> left;
  std::string op;
  std::unique_ptr<Expression> right;
  BinaryExpression(std::unique_ptr<Expression> lhs, std::string oper, std::unique_ptr<Expression> rhs);
  std::string repr() const override;
};

struct ListLiteral final : public Expression {
  std::vector<std::unique_ptr<Expression>> elements;
  std::string repr() const override;
};

struct RangeExpression final : public Expression {
  std::unique_ptr<Expression> start;
  std::unique_ptr<Expression> end;
  std::unique_ptr<Expression> step;
  RangeExpression(std::unique_ptr<Expression> startExpr, std::unique_ptr<Expression> endExpr,
                  std::unique_ptr<Expression> stepExpr);
  std::string repr() const override;
};

struct AssignmentStatement final : public Statement {
  std::string name;
  std::string op;
  std::unique_ptr<Expression> expression;
  int line{0};
  int column{0};
  AssignmentStatement(std::string n, std::string oper, std::unique_ptr<Expression> expr, int lineNo = 0,
                      int columnNo = 0);
};

struct PrintStatement final : public Statement {
  std::unique_ptr<Expression> expression;
  explicit PrintStatement(std::unique_ptr<Expression> expr);
};

struct BlockStatement final : public Statement {
  std::vector<std::unique_ptr<Statement>> statements;
};

struct ForStatement final : public Statement {
  std::string loopVar;
  std::unique_ptr<Expression> iterable;
  std::unique_ptr<Statement> body;
  int loopVarLine{0};
  int loopVarColumn{0};
  ForStatement(std::string var, std::unique_ptr<Expression> iter, std::unique_ptr<Statement> stmt,
               int lineNo = 0, int columnNo = 0);
};

struct IfStatement final : public Statement {
  std::unique_ptr<Expression> condition;
  std::unique_ptr<Statement> thenBranch;
  std::unique_ptr<Statement> elseBranch;
  IfStatement(std::unique_ptr<Expression> conditionExpr, std::unique_ptr<Statement> thenStmt,
              std::unique_ptr<Statement> elseStmt);
};

struct Program final : public Node {
  std::vector<std::unique_ptr<Statement>> statements;
};

}  // namespace cd
