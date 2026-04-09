#include "compiler/parser.hpp"

#include <sstream>

namespace cd {

Program TopDownParser::parse() {
  Program program;
  consumeSeparators();
  while (!check(TokenType::ENDMARKER)) {
    program.statements.push_back(statement());
    consumeSeparators();
  }
  return program;
}

std::unique_ptr<Statement> TopDownParser::statement() {
  if (matchKeyword("print")) return printStatement();
  if (matchKeyword("for")) return forStatement();
  if (matchKeyword("if")) return ifStatement();
  if (check(TokenType::IDENTIFIER)) return assignmentStatement();
  throw errorExpected("Unexpected token while parsing statement", {"print", "for", "if", "IDENT"}, "Stmt");
}

std::unique_ptr<Statement> TopDownParser::printStatement() {
  consumePunctuator("(", "Expected '(' after print");
  auto expr = expression();
  consumePunctuator(")", "Expected ')' after print expression");
  return std::make_unique<PrintStatement>(std::move(expr));
}

std::unique_ptr<Statement> TopDownParser::forStatement() {
  const auto& name = consume(TokenType::IDENTIFIER, "Expected loop variable");
  consumeKeyword("in", "Expected 'in' in for statement");
  auto iter = expression();
  consumePunctuator(":", "Expected ':' after iterable");
  if (match({TokenType::NEWLINE})) {
    return std::make_unique<ForStatement>(name.lexeme, std::move(iter), blockStatement(), name.line, name.column);
  }
  auto body = statement();
  return std::make_unique<ForStatement>(name.lexeme, std::move(iter), std::move(body), name.line, name.column);
}

std::unique_ptr<Statement> TopDownParser::ifStatement() {
  auto condition = expression();
  consumePunctuator(":", "Expected ':' after if condition");
  consume(TokenType::NEWLINE, "Expected newline after if ':'");
  auto thenBranch = blockStatement();

  std::unique_ptr<Statement> elseBranch = nullptr;
  if (matchKeyword("else")) {
    consumePunctuator(":", "Expected ':' after else");
    consume(TokenType::NEWLINE, "Expected newline after else ':'");
    elseBranch = blockStatement();
  }

  return std::make_unique<IfStatement>(std::move(condition), std::move(thenBranch), std::move(elseBranch));
}

std::unique_ptr<Statement> TopDownParser::blockStatement() {
  consume(TokenType::INDENT, "Expected INDENT to start for-loop block");
  auto block = std::make_unique<BlockStatement>();
  consumeSeparators();
  while (!check(TokenType::DEDENT) && !check(TokenType::ENDMARKER)) {
    block->statements.push_back(statement());
    consumeSeparators();
  }
  consume(TokenType::DEDENT, "Expected DEDENT to close for-loop block");
  return block;
}

std::unique_ptr<Statement> TopDownParser::assignmentStatement() {
  const auto& name = consume(TokenType::IDENTIFIER, "Expected identifier");
  std::string op;
  if (matchOperator("=")) {
    op = "=";
  } else if (matchOperator("+=")) {
    op = "+=";
  } else if (matchOperator("-=")) {
    op = "-=";
  } else {
    throw errorExpected("Expected assignment operator", {"=", "+=", "-="}, "AssignStmt");
  }
  auto expr = expression();
  return std::make_unique<AssignmentStatement>(name.lexeme, op, std::move(expr), name.line, name.column);
}

std::unique_ptr<Expression> TopDownParser::expression() {
  auto expr = term();
  while (matchOperator("+") || matchOperator("-")) {
    std::string op = previous().lexeme;
    auto right = term();
    expr = std::make_unique<BinaryExpression>(std::move(expr), op, std::move(right));
  }
  return expr;
}

std::unique_ptr<Expression> TopDownParser::term() {
  auto expr = factor();
  while (matchOperator("*") || matchOperator("/")) {
    std::string op = previous().lexeme;
    auto right = factor();
    expr = std::make_unique<BinaryExpression>(std::move(expr), op, std::move(right));
  }
  return expr;
}

std::unique_ptr<Expression> TopDownParser::factor() {
  if (matchOperator("-")) {
    std::string op = previous().lexeme;
    return std::make_unique<UnaryExpression>(op, factor());
  }
  return primary();
}

std::unique_ptr<Expression> TopDownParser::primary() {
  if (match({TokenType::NUMBER})) return std::make_unique<IntLiteral>(std::stoi(previous().lexeme));
  if (match({TokenType::FLOAT})) return std::make_unique<FloatLiteral>(std::stod(previous().lexeme));
  if (match({TokenType::STRING})) return std::make_unique<StringLiteral>(previous().lexeme);
  if (match({TokenType::BOOLEAN})) return std::make_unique<BoolLiteral>(previous().lexeme == "true");
  if (match({TokenType::IDENTIFIER})) return std::make_unique<Identifier>(previous().lexeme);
  if (matchKeyword("range")) {
    consumePunctuator("(", "Expected '(' after range");
    auto start = expression();
    consumePunctuator(",", "Expected ',' after range start");
    auto end = expression();
    std::unique_ptr<Expression> stepExpr = nullptr;
    if (matchPunctuator(",")) {
      stepExpr = expression();
    }
    consumePunctuator(")", "Expected ')' after range arguments");
    return std::make_unique<RangeExpression>(std::move(start), std::move(end), std::move(stepExpr));
  }
  if (matchPunctuator("(")) {
    auto expr = expression();
    consumePunctuator(")", "Expected ')' after expression");
    return expr;
  }
  if (matchPunctuator("[")) {
    auto list = std::make_unique<ListLiteral>();
    if (!checkPunctuator("]")) {
      list->elements.push_back(expression());
      while (matchPunctuator(",")) {
        list->elements.push_back(expression());
      }
    }
    consumePunctuator("]", "Expected ']' after list");
    return list;
  }
  throw errorExpected("Unexpected token while parsing primary expression",
                      {"INT", "FLOAT", "STRING", "BOOL", "IDENT", "range", "(", "["}, "Primary");
}

void TopDownParser::consumeSeparators() {
  while (match({TokenType::NEWLINE}) || matchPunctuator(";")) {
  }
}

bool TopDownParser::checkKeyword(const std::string& lexeme) const {
  return check(TokenType::KEYWORD) && peek().lexeme == lexeme;
}

bool TopDownParser::checkOperator(const std::string& lexeme) const {
  return check(TokenType::OPERATOR) && peek().lexeme == lexeme;
}

bool TopDownParser::checkPunctuator(const std::string& lexeme) const {
  return check(TokenType::PUNCTUATOR) && peek().lexeme == lexeme;
}

bool TopDownParser::matchKeyword(const std::string& lexeme) {
  if (checkKeyword(lexeme)) {
    advance();
    return true;
  }
  return false;
}

bool TopDownParser::matchOperator(const std::string& lexeme) {
  if (checkOperator(lexeme)) {
    advance();
    return true;
  }
  return false;
}

bool TopDownParser::matchPunctuator(const std::string& lexeme) {
  if (checkPunctuator(lexeme)) {
    advance();
    return true;
  }
  return false;
}

const Token& TopDownParser::consumeKeyword(const std::string& lexeme, const std::string& message) {
  if (checkKeyword(lexeme)) return advance();
  throw errorExpected(message, {lexeme}, "Keyword");
}

const Token& TopDownParser::consumeOperator(const std::string& lexeme, const std::string& message) {
  if (checkOperator(lexeme)) return advance();
  throw errorExpected(message, {lexeme}, "Operator");
}

const Token& TopDownParser::consumePunctuator(const std::string& lexeme, const std::string& message) {
  if (checkPunctuator(lexeme)) return advance();
  throw errorExpected(message, {lexeme}, "Punctuator");
}

bool TopDownParser::match(std::initializer_list<TokenType> types) {
  for (auto type : types) {
    if (check(type)) {
      advance();
      return true;
    }
  }
  return false;
}

const Token& TopDownParser::consume(TokenType type, const std::string& message) {
  if (check(type)) return advance();
  throw errorExpected(message, {tokenTypeName(type)}, "Token");
}

bool TopDownParser::check(TokenType type) const {
  if (isAtEnd()) return type == TokenType::ENDMARKER;
  return peek().type == type;
}

const Token& TopDownParser::advance() {
  if (!isAtEnd()) ++current_;
  return previous();
}

bool TopDownParser::isAtEnd() const { return peek().type == TokenType::ENDMARKER; }
const Token& TopDownParser::peek() const { return tokens_[current_]; }
const Token& TopDownParser::previous() const { return tokens_[current_ - 1]; }

ParseError TopDownParser::errorExpected(const std::string& message,
                                        const std::vector<std::string>& expected,
                                        const std::string& context) const {
  const auto& t = peek();
  std::ostringstream out;
  out << message << " at line " << t.line << ", column " << t.column << ". Found '" << t.lexeme << "'";
  if (!expected.empty()) {
    out << ". Expected one of: ";
    for (std::size_t i = 0; i < expected.size(); ++i) {
      out << expected[i];
      if (i + 1 < expected.size()) out << ", ";
    }
  }
  out << ". Context: " << context;
  return ParseError(out.str(), t.line, t.column, t.lexeme, expected, context);
}

}  // namespace cd
