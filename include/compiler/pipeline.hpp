#pragma once

#include <memory>
#include <vector>

#include "compiler/ast_nodes.hpp"
#include "compiler/grammar_tools.hpp"
#include "compiler/symbol_table.hpp"
#include "compiler/tokens.hpp"

namespace cd {

struct CompilationResult {
  std::vector<Token> tokens;
  std::vector<ScopeSnapshot> symbolSnapshot;
  std::vector<std::string> semanticErrors;
  GrammarArtifacts grammarArtifacts;
  Program ast;
  std::shared_ptr<SymbolTableManager> symbolTableOwner;
};

class CompilerPipeline {
 public:
  CompilationResult run(const std::string& source) const;
};

}  // namespace cd
