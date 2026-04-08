#pragma once

#include <unordered_map>
#include <vector>

#include "compiler/ast_nodes.hpp"
#include "compiler/grammar_tools.hpp"
#include "compiler/symbol_table.hpp"
#include "compiler/tokens.hpp"

namespace cd {

struct CompilationResult {
  std::vector<Token> tokens;
  std::unordered_map<std::string, SymbolRecord> symbolSnapshot;
  GrammarArtifacts grammarArtifacts;
  Program ast;
};

class CompilerPipeline {
 public:
  CompilationResult run(const std::string& source) const;
};

}  // namespace cd
