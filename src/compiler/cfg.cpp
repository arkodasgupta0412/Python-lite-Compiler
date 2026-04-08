#include "compiler/cfg.hpp"

namespace cd {

const std::string EPSILON = "epsilon";

CFGDefinition CFGProvider::rawCFG() {
  CFGDefinition cfg;
  cfg.startSymbol = "Program";
  cfg.productions = {
      {"Program", {{"StmtList", "EOF"}}},
      {"StmtList", {{"StmtTail"}}},
      // TopDownParser uses consumeSeparators() (zero or more) between
      // statements, so separator runs and direct adjacency are both legal.
      {"StmtTail", {{"SEP", "StmtTail"}, {"Stmt", "StmtTail"}, {EPSILON}}},
      {"Stmt", {{"AssignStmt"}, {"PrintStmt"}, {"ForStmt"}, {"IfStmt"}}},
      {"AssignStmt", {{"IDENT", "AssignOp", "Expr"}}},
      {"AssignOp", {{"ASSIGN"}, {"PLUS_ASSIGN"}, {"MINUS_ASSIGN"}}},
      {"PrintStmt", {{"PRINT", "LPAREN", "Expr", "RPAREN"}}},
      {"ForStmt", {{"FOR", "IDENT", "IN", "Expr", "COLON", "ForBody"}}},
      {"ForBody", {{"Stmt"}, {"NEWLINE", "INDENT", "StmtList", "DEDENT"}}},
      {"IfStmt", {{"IF", "Expr", "COLON", "NEWLINE", "INDENT", "StmtList", "DEDENT", "ElseOpt"}}},
      {"ElseOpt", {{"ELSE", "COLON", "NEWLINE", "INDENT", "StmtList", "DEDENT"}, {EPSILON}}},
      {"Expr", {{"Expr", "PLUS", "Term"}, {"Expr", "MINUS", "Term"}, {"Term"}}},
      {"Term", {{"Term", "STAR", "Factor"}, {"Term", "SLASH", "Factor"}, {"Factor"}}},
      {"Factor", {{"MINUS", "Factor"}, {"Primary"}}},
      {"Primary",
       {{"INT"},
        {"FLOAT"},
        {"STRING"},
        {"BOOL"},
        {"IDENT"},
        {"RangeCall"},
        {"LPAREN", "Expr", "RPAREN"},
        {"ListLiteral"}}},
      {"RangeCall", {{"RANGE", "LPAREN", "Expr", "COMMA", "Expr", "RangeStepOpt", "RPAREN"}}},
      {"RangeStepOpt", {{"COMMA", "Expr"}, {EPSILON}}},
      {"ListLiteral", {{"LBRACKET", "ElementsOpt", "RBRACKET"}}},
      {"ElementsOpt", {{"Expr", "MoreElements"}, {EPSILON}}},
      {"MoreElements", {{"COMMA", "Expr", "MoreElements"}, {EPSILON}}},
      {"SEP", {{"NEWLINE"}, {"SEMICOLON"}}},
  };
  return cfg;
}

}  // namespace cd
