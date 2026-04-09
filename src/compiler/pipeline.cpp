#include "compiler/pipeline.hpp"

#include "compiler/cfg.hpp"
#include "compiler/lexer.hpp"
#include "compiler/parser.hpp"
#include "compiler/semantic.hpp"
#include "compiler/symbol_table.hpp"

namespace cd {

CompilationResult CompilerPipeline::run(const std::string& source) const {
  Lexer lexer(source);
  auto tokens = lexer.tokenize();

  TopDownParser parser(tokens);
  auto program = parser.parse();

  auto symbolTable = std::make_shared<SymbolTableManager>();
  SemanticAnalyzer semantic(*symbolTable);
  std::vector<std::string> semanticErrors = semantic.analyze(program);

  auto cfg = CFGProvider::rawCFG();
  auto artifacts = analyzeCFG(cfg);
  artifacts.ll1PanicParse = parseLL1WithPanicMode(tokens, cfg.startSymbol, artifacts.transformedGrammar,
                                                  artifacts.follow, artifacts.parseTable);

  return CompilationResult{tokens, symbolTable->snapshotScopes(), semanticErrors, artifacts, std::move(program),
                           symbolTable};
}

}  // namespace cd
