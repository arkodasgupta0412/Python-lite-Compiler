#include <exception>
#include <iostream>
#include <string>

#include "compiler/cfg.hpp"
#include "compiler/grammar_tools.hpp"
#include "compiler/io_utils.hpp"
#include "compiler/lexer.hpp"
#include "compiler/parser.hpp"
#include "compiler/pipeline.hpp"
#include "compiler/semantic.hpp"

int main(int argc, char* argv[]) {
  cd::OutputWriter writer;
  std::string source;
  try {
    cd::ProgramReader reader;
    cd::CompilerPipeline pipeline;
    std::string exePath = (argc > 0 && argv[0] != nullptr) ? argv[0] : "";
    std::size_t slashPos = exePath.find_last_of("/\\");
    std::string exeDir = (slashPos == std::string::npos) ? "." : exePath.substr(0, slashPos);
    std::string docsDir = exeDir + "/docs";

    std::string path = (argc > 1) ? argv[1] : "examples/sample_program.cd";
    source = reader.read(path);
    cd::Lexer lexer(source);
    auto tokens = lexer.tokenize();

    auto cfg = cd::CFGProvider::rawCFG();
    auto artifacts = cd::analyzeCFG(cfg);
    artifacts.ll1PanicParse = cd::parseLL1WithPanicMode(tokens, cfg.startSymbol, artifacts.transformedGrammar,
                                                        artifacts.follow, artifacts.parseTable);

    writer.write("Lexical conflicts: 0");
    writer.write("Parse table conflicts: " + std::to_string(artifacts.conflicts.size()));
    for (const auto& c : artifacts.conflicts) {
      writer.write("  " + c);
    }

    writer.writeTokenStreamReport(docsDir + "/token_stream.txt", tokens);
    writer.writeTokenStreamImage(docsDir + "/token_stream.svg", tokens);
    writer.writeParseTableReport(docsDir + "/parse_table.txt", artifacts);
    writer.writeGrammarImages(docsDir, artifacts);
    writer.writeGrammarRulesImage(docsDir + "/grammar.svg", cd::CFGProvider::rawCFG());
    writer.write("Panic-mode recovered errors: " + std::to_string(artifacts.ll1PanicParse.errorsRecovered));
    for (const auto& action : artifacts.ll1PanicParse.actionLog) {
      writer.write("  " + action);
    }

    writer.write("Token stream report written to docs/token_stream.txt");
    writer.write("Token stream image written to docs/token_stream.svg");
    writer.write("Parse table report written to docs/parse_table.txt");
    writer.write("Grammar images written to docs/first_set.svg, docs/follow_set.svg, docs/parse_table.svg");
    writer.write("Panic-mode image written to docs/panic_recovery.svg");
    writer.write("Grammar rules image written to docs/grammar.svg");
    try {
      auto result = pipeline.run(source);
      writer.writeSymbolTableReport(docsDir + "/symbol_table.txt", result.symbolSnapshot);
      writer.writeSymbolTableImage(docsDir + "/symbol_table.svg", result.symbolSnapshot);
      writer.write("Symbol table report written to docs/symbol_table.txt");
      writer.write("Symbol table image written to docs/symbol_table.svg");

      writer.writeASTDot(docsDir + "/ast.dot", result.ast);
      bool astSvgRendered = writer.renderDotToSvg(docsDir + "/ast.dot", docsDir + "/ast.svg");
      writer.write("AST Graphviz file written to docs/ast.dot");
      if (astSvgRendered) {
        writer.write("AST image written to docs/ast.svg");
      } else {
        writer.write("AST SVG not generated (Graphviz 'dot' command not found).");
      }
    } catch (const cd::ParseError& ex) {
      int line = ex.line();
      int column = ex.column();
      if (line <= 0 || column <= 0) cd::OutputWriter::extractLineColumn(ex.what(), line, column);
      writer.writeDiagnosticWithSnippet("Parse Error", ex.what(), source, line, column);
      writer.write("Skipping symbol table and AST outputs because strict parse failed.");
    } catch (const cd::SemanticError& ex) {
      int line = -1;
      int column = -1;
      cd::OutputWriter::extractLineColumn(ex.what(), line, column);
      writer.writeDiagnosticWithSnippet("Semantic Error", ex.what(), source, line, column);
      writer.write("Skipping symbol table and AST outputs because semantic analysis failed.");
    }
    return 0;
  } catch (const cd::LexicalError& ex) {
    int line = -1;
    int column = -1;
    cd::OutputWriter::extractLineColumn(ex.what(), line, column);
    writer.writeDiagnosticWithSnippet("Lexical Error", ex.what(), source, line, column);
    return 1;
  } catch (const std::exception& ex) {
    std::cerr << "Error: " << ex.what() << "\n";
    return 1;
  }
}
