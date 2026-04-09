#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "compiler/cfg.hpp"
#include "compiler/grammar_tools.hpp"
#include "compiler/io_utils.hpp"
#include "compiler/lexer.hpp"
#include "compiler/parser.hpp"
#include "compiler/pipeline.hpp"
#include "compiler/semantic.hpp"
#include "compiler/symbol_table.hpp"

#ifndef CD_PROJECT_SOURCE_DIR
#define CD_PROJECT_SOURCE_DIR "."
#endif

namespace {

void require(bool condition, const std::string& message) {
  if (!condition) throw std::runtime_error(message);
}

std::string readTextFile(const std::filesystem::path& path) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("Failed to read file: " + path.string());
  std::ostringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}

void assertFileEquals(const std::filesystem::path& golden, const std::filesystem::path& actual) {
  const std::string expected = readTextFile(golden);
  const std::string got = readTextFile(actual);
  if (expected == got) return;

  std::istringstream expIn(expected);
  std::istringstream gotIn(got);
  std::string expLine;
  std::string gotLine;
  int lineNo = 1;
  while (true) {
    const bool hasExp = static_cast<bool>(std::getline(expIn, expLine));
    const bool hasGot = static_cast<bool>(std::getline(gotIn, gotLine));
    if (!hasExp && !hasGot) break;
    if (!hasExp || !hasGot || expLine != gotLine) {
      throw std::runtime_error("File mismatch at line " + std::to_string(lineNo) + " for " + actual.string());
    }
    ++lineNo;
  }
}

void testLexerBasic() {
  const std::string src = "x = 1\nprint(x)\n";
  cd::Lexer lexer(src);
  auto tokens = lexer.tokenize();

  require(!tokens.empty(), "Lexer produced empty token stream");
  require(tokens[0].type == cd::TokenType::IDENTIFIER && tokens[0].lexeme == "x", "First token should be identifier x");

  bool sawPrint = false;
  for (const auto& tk : tokens) {
    if (tk.type == cd::TokenType::KEYWORD && tk.lexeme == "print") {
      sawPrint = true;
      break;
    }
  }
  require(sawPrint, "Expected print keyword token");
  require(tokens.back().type == cd::TokenType::ENDMARKER, "Token stream must end with ENDMARKER");
}

void testParserBasic() {
  const std::string src = "x = 1\nprint(x + 2)\n";
  cd::Lexer lexer(src);
  auto tokens = lexer.tokenize();

  cd::TopDownParser parser(tokens);
  cd::Program program = parser.parse();
  require(program.statements.size() == 2, "Parser should produce 2 top-level statements");
}

void testGrammarFirstFollowAndTable() {
  const auto cfg = cd::CFGProvider::rawCFG();
  auto artifacts = cd::analyzeCFG(cfg);

  require(artifacts.first.count("Program") == 1, "FIRST(Program) missing");
  require(artifacts.first["Program"].find(cd::EPSILON) == artifacts.first["Program"].end(),
          "FIRST(Program) must not contain epsilon");
  require(artifacts.conflicts.empty(), "Parse table conflicts detected");
  require(artifacts.ll1Validation.valid, "LL(1) validation failed");

  const auto rowIt = artifacts.parseTable.find("Program");
  require(rowIt != artifacts.parseTable.end(), "Program row missing in parse table");
  const auto cellIt = rowIt->second.find("EOF");
  require(cellIt != rowIt->second.end(), "M[Program, EOF] missing");
  require(!(cellIt->second.size() == 1 && cellIt->second[0] == cd::EPSILON),
          "M[Program, EOF] should not be epsilon production");
}

void testSemanticBasic() {
  const std::string src = "x = 1\ny = 2\nprint(x + y)\n";
  cd::CompilerPipeline pipeline;
  auto result = pipeline.run(src);

  require(result.symbolSnapshot.count("x") == 1, "Symbol table missing x");
  require(result.symbolSnapshot.count("y") == 1, "Symbol table missing y");
  require(result.symbolSnapshot.at("x").varType == "int", "x type should be int");
}

void testNegativeLexicalError() {
  const std::string src = "x = @\n";
  cd::Lexer lexer(src);
  bool threw = false;
  try {
    auto _ = lexer.tokenize();
    (void)_;
  } catch (const cd::LexicalError&) {
    threw = true;
  }
  require(threw, "Expected lexical error for invalid character");
}

void testNegativeSyntaxError() {
  const std::string src = "print(x +++++++ y)\n";
  cd::Lexer lexer(src);
  auto tokens = lexer.tokenize();

  bool threw = false;
  try {
    cd::TopDownParser parser(tokens);
    auto _ = parser.parse();
    (void)_;
  } catch (const cd::ParseError&) {
    threw = true;
  }
  require(threw, "Expected parse error for malformed plus sequence");
}

void testNegativeSemanticError() {
  const std::string src = "print(x)\n";
  cd::CompilerPipeline pipeline;
  bool threw = false;
  try {
    auto _ = pipeline.run(src);
    (void)_;
  } catch (const cd::SemanticError&) {
    threw = true;
  }
  require(threw, "Expected semantic error for undefined identifier");
}

void testGoldenReports() {
  const std::string src = "x = 1\ny = 2\nprint(x + y)\n";

  cd::OutputWriter writer;
  cd::Lexer lexer(src);
  auto tokens = lexer.tokenize();

  const auto cfg = cd::CFGProvider::rawCFG();
  auto artifacts = cd::analyzeCFG(cfg);
  artifacts.ll1PanicParse = cd::parseLL1WithPanicMode(tokens, cfg.startSymbol, artifacts.transformedGrammar,
                                                      artifacts.follow, artifacts.parseTable);

  cd::CompilerPipeline pipeline;
  auto result = pipeline.run(src);

  const std::filesystem::path base = CD_PROJECT_SOURCE_DIR;
  const std::filesystem::path outDir = base / "tests" / "out";
  const std::filesystem::path goldenDir = base / "tests" / "golden";
  std::filesystem::create_directories(outDir);

  const auto parseTableOut = outDir / "parse_table.txt";
  const auto tokenStreamOut = outDir / "token_stream.txt";
  const auto symbolTableOut = outDir / "symbol_table.txt";

  writer.writeParseTableReport(parseTableOut.string(), artifacts);
  writer.writeTokenStreamReport(tokenStreamOut.string(), tokens);
  writer.writeSymbolTableReport(symbolTableOut.string(), result.symbolSnapshot);

  assertFileEquals(goldenDir / "parse_table.txt", parseTableOut);
  assertFileEquals(goldenDir / "token_stream.txt", tokenStreamOut);
  assertFileEquals(goldenDir / "symbol_table.txt", symbolTableOut);
}

}  // namespace

int main() {
  struct TestCase {
    const char* name;
    void (*fn)();
  };

  const std::vector<TestCase> tests = {
      {"lexer-basic", testLexerBasic},
      {"parser-basic", testParserBasic},
      {"grammar-first-follow-parse-table", testGrammarFirstFollowAndTable},
      {"semantic-basic", testSemanticBasic},
      {"negative-lexical", testNegativeLexicalError},
      {"negative-syntax", testNegativeSyntaxError},
      {"negative-semantic", testNegativeSemanticError},
      {"golden-reports", testGoldenReports},
  };

  int passed = 0;
  for (const auto& test : tests) {
    try {
      test.fn();
      ++passed;
      std::cout << "[PASS] " << test.name << "\n";
    } catch (const std::exception& ex) {
      std::cerr << "[FAIL] " << test.name << ": " << ex.what() << "\n";
      return 1;
    }
  }

  std::cout << "Passed " << passed << " tests.\n";
  return 0;
}
