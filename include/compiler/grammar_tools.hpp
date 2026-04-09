#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "compiler/cfg.hpp"
#include "compiler/tokens.hpp"

namespace cd {

using GrammarMap = std::unordered_map<std::string, std::vector<std::vector<std::string>>>;
using SetMap = std::unordered_map<std::string, std::unordered_set<std::string>>;
using ParseTable = std::unordered_map<std::string, std::unordered_map<std::string, std::vector<std::string>>>;

struct LL1PanicParseResult {
  bool accepted{false};
  std::size_t errorsRecovered{0};
  std::vector<std::string> actionLog;
};

struct LL1ValidationResult {
  bool valid{true};
  std::vector<std::string> firstFirstConflicts;
  std::vector<std::string> firstFollowConflicts;
};

struct GrammarArtifacts {
  GrammarMap transformedGrammar;
  SetMap first;
  SetMap follow;
  ParseTable parseTable;
  std::vector<std::string> conflicts;
  LL1ValidationResult ll1Validation;
  LL1PanicParseResult ll1PanicParse;
};

class GrammarTransformer {
 public:
  explicit GrammarTransformer(GrammarMap grammar) : grammar_(std::move(grammar)) {}
  GrammarMap removeLeftRecursion();
  GrammarMap leftFactor();

 private:
  GrammarMap grammar_;
  static std::vector<std::string> longestCommonPrefix(const std::vector<std::vector<std::string>>& productions);
};

class FirstFollowAnalyzer {
 public:
  FirstFollowAnalyzer(GrammarMap grammar, std::string startSymbol)
      : grammar_(std::move(grammar)), startSymbol_(std::move(startSymbol)) {}

  SetMap computeFirst() const;
  SetMap computeFollow(const SetMap& first) const;
  std::pair<ParseTable, std::vector<std::string>> buildParseTable(const SetMap& first,
                                                                  const SetMap& follow) const;

 private:
  GrammarMap grammar_;
  std::string startSymbol_;
  bool isNonTerminal(const std::string& symbol) const;
  std::unordered_set<std::string> firstOfSequence(const std::vector<std::string>& sequence,
                                                  const SetMap& first) const;
};

GrammarArtifacts analyzeCFG(const CFGDefinition& cfg);
LL1PanicParseResult parseLL1WithPanicMode(const std::vector<Token>& tokens,
                                          const std::string& startSymbol,
                                          const GrammarMap& grammar,
                                          const SetMap& follow,
                                          const ParseTable& parseTable);

}  // namespace cd
