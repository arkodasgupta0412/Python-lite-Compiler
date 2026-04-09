#include "compiler/parser.hpp"

namespace cd {

Program TopDownParser::parse() {
  Program program;
  consumeSeparators();
  while (!check(TokenType::END_OF_FILE)) {
    program.statements.push_back(statement());
    consumeSeparators();
  }
  return program;
}

std::unique_ptr<Statement> TopDownParser::statement() {
  if (match({TokenType::PRINT})) return printStatement();
  if (match({TokenType::FOR})) return forStatement();
  if (match({TokenType::IF})) return ifStatement();
  if (check(TokenType::IDENT)) return assignmentStatement();
  const auto& t = peek();
  throw ParseError("Unexpected token '" + t.lexeme + "' at line " + std::to_string(t.line) + ", column " +
                   std::to_string(t.column));
}

std::unique_ptr<Statement> TopDownParser::printStatement() {
  consume(TokenType::LPAREN, "Expected '(' after print");
  auto expr = expression();
  consume(TokenType::RPAREN, "Expected ')' after print expression");
  return std::make_unique<PrintStatement>(std::move(expr));
}

std::unique_ptr<Statement> TopDownParser::forStatement() {
  const auto& name = consume(TokenType::IDENT, "Expected loop variable");
  consume(TokenType::IN, "Expected 'in' in for statement");
  auto iter = expression();
  consume(TokenType::COLON, "Expected ':' after iterable");
  if (match({TokenType::NEWLINE})) {
    return std::make_unique<ForStatement>(name.lexeme, std::move(iter), blockStatement(), name.line, name.column);
  }
  auto body = statement();
  return std::make_unique<ForStatement>(name.lexeme, std::move(iter), std::move(body), name.line, name.column);
}

std::unique_ptr<Statement> TopDownParser::ifStatement() {
  auto condition = expression();
  consume(TokenType::COLON, "Expected ':' after if condition");
  consume(TokenType::NEWLINE, "Expected newline after if ':'");
  auto thenBranch = blockStatement();

  std::unique_ptr<Statement> elseBranch = nullptr;
  if (match({TokenType::ELSE})) {
    consume(TokenType::COLON, "Expected ':' after else");
    consume(TokenType::NEWLINE, "Expected newline after else ':'");
    elseBranch = blockStatement();
  }

  return std::make_unique<IfStatement>(std::move(condition), std::move(thenBranch), std::move(elseBranch));
}

std::unique_ptr<Statement> TopDownParser::blockStatement() {
  consume(TokenType::INDENT, "Expected INDENT to start for-loop block");
  auto block = std::make_unique<BlockStatement>();
  consumeSeparators();
  while (!check(TokenType::DEDENT) && !check(TokenType::END_OF_FILE)) {
    block->statements.push_back(statement());
    consumeSeparators();
  }
  consume(TokenType::DEDENT, "Expected DEDENT to close for-loop block");
  return block;
}

std::unique_ptr<Statement> TopDownParser::assignmentStatement() {
  const auto& name = consume(TokenType::IDENT, "Expected identifier");
  std::string op;
  if (match({TokenType::ASSIGN})) {
    op = "=";
  } else if (match({TokenType::PLUS_ASSIGN})) {
    op = "+=";
  } else if (match({TokenType::MINUS_ASSIGN})) {
    op = "-=";
  } else {
    const auto& t = peek();
    throw ParseError("Expected assignment operator at line " + std::to_string(t.line) + ", column " +
                     std::to_string(t.column));
  }
  auto expr = expression();
  return std::make_unique<AssignmentStatement>(name.lexeme, op, std::move(expr), name.line, name.column);
}

std::unique_ptr<Expression> TopDownParser::expression() {
  auto expr = term();
  while (match({TokenType::PLUS, TokenType::MINUS})) {
    std::string op = previous().lexeme;
    auto right = term();
    expr = std::make_unique<BinaryExpression>(std::move(expr), op, std::move(right));
  }
  return expr;
}

std::unique_ptr<Expression> TopDownParser::term() {
  auto expr = factor();
  while (match({TokenType::STAR, TokenType::SLASH})) {
    std::string op = previous().lexeme;
    auto right = factor();
    expr = std::make_unique<BinaryExpression>(std::move(expr), op, std::move(right));
  }
  return expr;
}

std::unique_ptr<Expression> TopDownParser::factor() {
  if (match({TokenType::MINUS})) {
    std::string op = previous().lexeme;
    return std::make_unique<UnaryExpression>(op, factor());
  }
  return primary();
}

std::unique_ptr<Expression> TopDownParser::primary() {
  if (match({TokenType::INT})) return std::make_unique<IntLiteral>(std::stoi(previous().lexeme));
  if (match({TokenType::FLOAT})) return std::make_unique<FloatLiteral>(std::stod(previous().lexeme));
  if (match({TokenType::STRING})) return std::make_unique<StringLiteral>(previous().lexeme);
  if (match({TokenType::BOOL})) return std::make_unique<BoolLiteral>(previous().lexeme == "true");
  if (match({TokenType::IDENT})) {
    const auto& ident = previous();
    return std::make_unique<Identifier>(ident.lexeme, ident.line, ident.column);
  }
  if (match({TokenType::RANGE})) {
    consume(TokenType::LPAREN, "Expected '(' after range");
    auto start = expression();
    consume(TokenType::COMMA, "Expected ',' after range start");
    auto end = expression();
    std::unique_ptr<Expression> stepExpr = nullptr;
    if (match({TokenType::COMMA})) {
      stepExpr = expression();
    }
    consume(TokenType::RPAREN, "Expected ')' after range arguments");
    return std::make_unique<RangeExpression>(std::move(start), std::move(end), std::move(stepExpr));
  }
  if (match({TokenType::LPAREN})) {
    auto expr = expression();
    consume(TokenType::RPAREN, "Expected ')' after expression");
    return expr;
  }
  if (match({TokenType::LBRACKET})) {
    auto list = std::make_unique<ListLiteral>();
    if (!check(TokenType::RBRACKET)) {
      list->elements.push_back(expression());
      while (match({TokenType::COMMA})) {
        list->elements.push_back(expression());
      }
    }
    consume(TokenType::RBRACKET, "Expected ']' after list");
    return list;
  }
  const auto& t = peek();
  throw ParseError("Unexpected token '" + t.lexeme + "' at line " + std::to_string(t.line) + ", column " +
                   std::to_string(t.column));
}

void TopDownParser::consumeSeparators() {
  while (match({TokenType::NEWLINE, TokenType::SEMICOLON})) {
  }
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
  const auto& t = peek();
  throw ParseError(message + " at line " + std::to_string(t.line) + ", column " + std::to_string(t.column));
}

bool TopDownParser::check(TokenType type) const {
  if (isAtEnd()) return type == TokenType::END_OF_FILE;
  return peek().type == type;
}

const Token& TopDownParser::advance() {
  if (!isAtEnd()) ++current_;
  return previous();
}

bool TopDownParser::isAtEnd() const { return peek().type == TokenType::END_OF_FILE; }
const Token& TopDownParser::peek() const { return tokens_[current_]; }
const Token& TopDownParser::previous() const { return tokens_[current_ - 1]; }

}  // namespace cd
