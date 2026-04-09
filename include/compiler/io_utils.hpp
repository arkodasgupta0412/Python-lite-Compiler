#pragma once

#include <string>
#include <vector>

#include "compiler/cfg.hpp"
#include "compiler/ast_nodes.hpp"
#include "compiler/grammar_tools.hpp"
#include "compiler/symbol_table.hpp"
#include "compiler/tokens.hpp"

namespace cd {

class ProgramReader {
 public:
  std::string read(const std::string& path) const;
};

class OutputWriter {
 public:
  void write(const std::string& text) const;
  void writeParseTableReport(const std::string& path, const GrammarArtifacts& artifacts) const;
  void writeGrammarImages(const std::string& outputDir, const GrammarArtifacts& artifacts) const;
  void writePanicRecoveryImage(const std::string& path, const LL1PanicParseResult& panicResult) const;
  void writeGrammarRulesImage(const std::string& path, const CFGDefinition& cfg) const;
  void writeTokenStreamReport(const std::string& path, const std::vector<Token>& tokens) const;
  void writeSymbolTableReport(const std::string& path, const std::vector<ScopeSnapshot>& symbolSnapshot) const;
  void writeTokenStreamImage(const std::string& path, const std::vector<Token>& tokens) const;
  void writeSymbolTableImage(const std::string& path, const std::vector<ScopeSnapshot>& symbolSnapshot) const;
  void writeASTDot(const std::string& path, const Program& program) const;
  bool renderDotToSvg(const std::string& dotPath, const std::string& svgPath) const;
};

}  // namespace cd
