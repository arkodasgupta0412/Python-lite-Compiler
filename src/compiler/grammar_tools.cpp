#include "compiler/grammar_tools.hpp"

#include <algorithm>
#include <set>
#include <sstream>

namespace {

bool isTerminalSymbol(const std::string& symbol,
                      const cd::GrammarMap& grammar,
                      const cd::SetMap& follow,
                      const cd::ParseTable& parseTable) {
  if (symbol == cd::EPSILON) return false;
  if (symbol == "EOF") return true;
  if (grammar.find(symbol) != grammar.end()) return false;
  if (follow.find(symbol) != follow.end()) return false;
  if (parseTable.find(symbol) != parseTable.end()) return false;
  return true;
}

std::string stackSnapshot(const std::vector<std::string>& stack) {
  std::ostringstream out;
  for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
    out << *it;
    if (it + 1 != stack.rend()) out << " ";
  }
  return out.str();
}

std::string productionToString(const std::vector<std::string>& production) {
  std::ostringstream out;
  for (std::size_t i = 0; i < production.size(); ++i) {
    out << production[i];
    if (i + 1 < production.size()) out << " ";
  }
  return out.str();
}

std::vector<std::string> sortedNonTerminals(const cd::GrammarMap& grammar) {
  std::vector<std::string> nts;
  nts.reserve(grammar.size());
  for (const auto& [k, _] : grammar) nts.push_back(k);
  std::sort(nts.begin(), nts.end());
  return nts;
}

std::set<std::string> sortedSetNoEpsilon(const std::unordered_set<std::string>& values) {
  std::set<std::string> out;
  for (const auto& s : values) {
    if (s != cd::EPSILON) out.insert(s);
  }
  return out;
}

std::unordered_set<std::string> firstOfSequenceLocal(const std::vector<std::string>& sequence,
                                                     const cd::GrammarMap& grammar,
                                                     const cd::SetMap& first) {
  auto isNonTerminal = [&grammar](const std::string& symbol) { return grammar.find(symbol) != grammar.end(); };

  std::unordered_set<std::string> result;
  if (sequence.empty()) return {cd::EPSILON};

  for (const auto& symbol : sequence) {
    if (symbol == cd::EPSILON) {
      result.insert(cd::EPSILON);
      break;
    }
    if (!isNonTerminal(symbol)) {
      result.insert(symbol);
      break;
    }
    const auto& firstSet = first.at(symbol);
    for (const auto& s : firstSet) {
      if (s != cd::EPSILON) result.insert(s);
    }
    if (firstSet.find(cd::EPSILON) == firstSet.end()) break;
  }

  bool allNullable = true;
  for (const auto& symbol : sequence) {
    if (symbol == cd::EPSILON) continue;
    if (!isNonTerminal(symbol) || first.at(symbol).find(cd::EPSILON) == first.at(symbol).end()) {
      allNullable = false;
      break;
    }
  }
  if (allNullable) result.insert(cd::EPSILON);
  return result;
}

std::string joinSorted(const std::set<std::string>& values) {
  std::ostringstream out;
  bool first = true;
  for (const auto& v : values) {
    if (!first) out << ", ";
    out << v;
    first = false;
  }
  return out.str();
}

cd::LL1ValidationResult validateLL1(const cd::GrammarMap& grammar,
                                    const cd::SetMap& first,
                                    const cd::SetMap& follow) {
  cd::LL1ValidationResult result;

  auto nts = sortedNonTerminals(grammar);
  for (const auto& nt : nts) {
    const auto gIt = grammar.find(nt);
    if (gIt == grammar.end()) continue;
    const auto& prods = gIt->second;

    std::vector<std::unordered_set<std::string>> seqFirst;
    seqFirst.reserve(prods.size());
    for (const auto& p : prods) {
      seqFirst.push_back(firstOfSequenceLocal(p, grammar, first));
    }

    for (std::size_t i = 0; i < prods.size(); ++i) {
      for (std::size_t j = i + 1; j < prods.size(); ++j) {
        auto lhs = sortedSetNoEpsilon(seqFirst[i]);
        auto rhs = sortedSetNoEpsilon(seqFirst[j]);
        std::set<std::string> inter;
        std::set_intersection(lhs.begin(), lhs.end(), rhs.begin(), rhs.end(), std::inserter(inter, inter.begin()));
        if (!inter.empty()) {
          result.valid = false;
          result.firstFirstConflicts.push_back(
              "FIRST/FIRST conflict for " + nt + " between productions [" + productionToString(prods[i]) +
              "] and [" + productionToString(prods[j]) + "] on {" + joinSorted(inter) + "}");
        }
      }
    }

    for (std::size_t i = 0; i < prods.size(); ++i) {
      if (seqFirst[i].find(cd::EPSILON) == seqFirst[i].end()) continue;

      std::set<std::string> unionOthers;
      for (std::size_t j = 0; j < prods.size(); ++j) {
        if (i == j) continue;
        auto other = sortedSetNoEpsilon(seqFirst[j]);
        unionOthers.insert(other.begin(), other.end());
      }

      std::set<std::string> followSet;
      const auto fIt = follow.find(nt);
      if (fIt != follow.end()) {
        followSet.insert(fIt->second.begin(), fIt->second.end());
      }

      std::set<std::string> inter;
      std::set_intersection(unionOthers.begin(), unionOthers.end(), followSet.begin(), followSet.end(),
                            std::inserter(inter, inter.begin()));
      if (!inter.empty()) {
        result.valid = false;
        result.firstFollowConflicts.push_back(
            "FIRST/FOLLOW conflict for nullable production " + nt + " -> [" + productionToString(prods[i]) +
            "] on {" + joinSorted(inter) + "}");
      }
    }
  }

  return result;
}

}  // namespace

namespace cd {

GrammarMap GrammarTransformer::removeLeftRecursion() {
  auto nts = sortedNonTerminals(grammar_);

  for (std::size_t i = 0; i < nts.size(); ++i) {
    const auto& ai = nts[i];
    for (std::size_t j = 0; j < i; ++j) {
      const auto& aj = nts[j];
      std::vector<std::vector<std::string>> replaced;
      for (const auto& prod : grammar_[ai]) {
        if (!prod.empty() && prod[0] == aj) {
          std::vector<std::string> suffix(prod.begin() + 1, prod.end());
          for (const auto& beta : grammar_[aj]) {
            std::vector<std::string> merged;
            if (beta.size() == 1 && beta[0] == EPSILON) {
              merged = suffix;
            } else {
              merged = beta;
              merged.insert(merged.end(), suffix.begin(), suffix.end());
            }
            if (merged.empty()) merged.push_back(EPSILON);
            replaced.push_back(std::move(merged));
          }
        } else {
          replaced.push_back(prod);
        }
      }
      grammar_[ai] = replaced;
    }

    std::vector<std::vector<std::string>> alpha, beta;
    for (const auto& prod : grammar_[ai]) {
      if (!prod.empty() && prod[0] == ai) {
        alpha.emplace_back(prod.begin() + 1, prod.end());
      } else {
        beta.push_back(prod);
      }
    }
    if (alpha.empty()) continue;

    std::string aiDash = ai + "'";
    while (grammar_.find(aiDash) != grammar_.end()) aiDash.push_back('\'');

    std::vector<std::vector<std::string>> aiProds;
    for (auto& b : beta) {
      std::vector<std::string> transformed;
      if (b.size() == 1 && b[0] == EPSILON) {
        transformed.push_back(aiDash);
      } else {
        transformed = b;
        transformed.push_back(aiDash);
      }
      aiProds.push_back(std::move(transformed));
    }
    grammar_[ai] = aiProds;

    std::vector<std::vector<std::string>> dashProds;
    for (auto& a : alpha) {
      a.push_back(aiDash);
      dashProds.push_back(a);
    }
    dashProds.push_back({EPSILON});
    grammar_[aiDash] = dashProds;
  }
  return grammar_;
}

GrammarMap GrammarTransformer::leftFactor() {
  bool changed = true;
  while (changed) {
    changed = false;
    const auto nts = sortedNonTerminals(grammar_);
    for (const auto& nt : nts) {
      const auto& prods = grammar_[nt];
      std::unordered_map<std::string, std::vector<std::vector<std::string>>> groups;
      for (const auto& p : prods) {
        std::string key = p.empty() ? EPSILON : p[0];
        groups[key].push_back(p);
      }

      std::vector<std::vector<std::string>> factorGroup;
      std::vector<std::string> groupKeys;
      groupKeys.reserve(groups.size());
      for (const auto& [key, _] : groups) groupKeys.push_back(key);
      std::sort(groupKeys.begin(), groupKeys.end());

      for (const auto& key : groupKeys) {
        const auto& grouped = groups[key];
        if (key != EPSILON && grouped.size() > 1) {
          factorGroup = grouped;
          break;
        }
      }
      if (factorGroup.empty()) continue;

      auto prefix = longestCommonPrefix(factorGroup);
      if (prefix.empty()) continue;

      std::string ntDash = nt + "_F";
      while (grammar_.find(ntDash) != grammar_.end()) ntDash += "_F";

      std::vector<std::vector<std::string>> remaining;
      for (const auto& p : prods) {
        if (std::find(factorGroup.begin(), factorGroup.end(), p) == factorGroup.end()) remaining.push_back(p);
      }
      auto pfx = prefix;
      pfx.push_back(ntDash);
      remaining.push_back(pfx);
      grammar_[nt] = remaining;

      std::vector<std::vector<std::string>> dashProds;
      for (const auto& p : factorGroup) {
        if (p.size() == prefix.size()) {
          dashProds.push_back({EPSILON});
        } else {
          dashProds.emplace_back(p.begin() + static_cast<long long>(prefix.size()), p.end());
        }
      }
      grammar_[ntDash] = dashProds;
      changed = true;
      break;
    }
  }
  return grammar_;
}

std::vector<std::string> GrammarTransformer::longestCommonPrefix(
    const std::vector<std::vector<std::string>>& productions) {
  if (productions.empty()) return {};
  auto prefix = productions[0];
  for (std::size_t i = 1; i < productions.size(); ++i) {
    std::size_t j = 0;
    while (j < prefix.size() && j < productions[i].size() && prefix[j] == productions[i][j]) ++j;
    prefix.resize(j);
    if (prefix.empty()) break;
  }
  return prefix;
}

bool FirstFollowAnalyzer::isNonTerminal(const std::string& symbol) const {
  return grammar_.find(symbol) != grammar_.end();
}

std::unordered_set<std::string> FirstFollowAnalyzer::firstOfSequence(const std::vector<std::string>& sequence,
                                                                     const SetMap& first) const {
  std::unordered_set<std::string> result;
  if (sequence.empty()) return {EPSILON};
  for (const auto& symbol : sequence) {
    if (symbol == EPSILON) {
      result.insert(EPSILON);
      break;
    }
    if (!isNonTerminal(symbol)) {
      result.insert(symbol);
      break;
    }
    const auto& firstSet = first.at(symbol);
    for (const auto& s : firstSet) {
      if (s != EPSILON) result.insert(s);
    }
    if (firstSet.find(EPSILON) == firstSet.end()) break;
  }
  bool allNullable = true;
  for (const auto& symbol : sequence) {
    if (symbol == EPSILON) continue;
    if (!isNonTerminal(symbol) || first.at(symbol).find(EPSILON) == first.at(symbol).end()) {
      allNullable = false;
      break;
    }
  }
  if (allNullable) result.insert(EPSILON);
  return result;
}

SetMap FirstFollowAnalyzer::computeFirst() const {
  SetMap first;
  for (const auto& [nt, _] : grammar_) first[nt] = {};
  bool changed = true;
  while (changed) {
    changed = false;
    for (const auto& [nt, prods] : grammar_) {
      for (const auto& p : prods) {
        auto seqFirst = firstOfSequence(p, first);
        auto before = first[nt].size();
        first[nt].insert(seqFirst.begin(), seqFirst.end());
        if (first[nt].size() != before) changed = true;
      }
    }
  }
  return first;
}

SetMap FirstFollowAnalyzer::computeFollow(const SetMap& first) const {
  SetMap follow;
  for (const auto& [nt, _] : grammar_) follow[nt] = {};
  follow[startSymbol_].insert("EOF");

  bool changed = true;
  while (changed) {
    changed = false;
    for (const auto& [lhs, prods] : grammar_) {
      for (const auto& p : prods) {
        std::unordered_set<std::string> trailer = follow[lhs];
        for (auto it = p.rbegin(); it != p.rend(); ++it) {
          const auto& symbol = *it;
          if (symbol == EPSILON) continue;
          if (isNonTerminal(symbol)) {
            auto before = follow[symbol].size();
            follow[symbol].insert(trailer.begin(), trailer.end());
            if (first.at(symbol).find(EPSILON) != first.at(symbol).end()) {
              std::unordered_set<std::string> merged = trailer;
              for (const auto& s : first.at(symbol)) {
                if (s != EPSILON) merged.insert(s);
              }
              trailer = merged;
            } else {
              trailer.clear();
              for (const auto& s : first.at(symbol)) {
                if (s != EPSILON) trailer.insert(s);
              }
            }
            if (follow[symbol].size() != before) changed = true;
          } else {
            trailer = {symbol};
          }
        }
      }
    }
  }
  return follow;
}

std::pair<ParseTable, std::vector<std::string>> FirstFollowAnalyzer::buildParseTable(const SetMap& first,
                                                                                      const SetMap& follow) const {
  ParseTable table;
  std::vector<std::string> conflicts;
  for (const auto& [nt, _] : grammar_) table[nt] = {};

  for (const auto& [nt, prods] : grammar_) {
    for (const auto& p : prods) {
      auto seqFirst = firstOfSequence(p, first);
      for (const auto& term : seqFirst) {
        if (term == EPSILON) continue;
        if (table[nt].count(term) && table[nt][term] != p) conflicts.push_back("Conflict at M[" + nt + ", " + term + "]");
        table[nt][term] = p;
      }
      if (seqFirst.find(EPSILON) != seqFirst.end()) {
        for (const auto& term : follow.at(nt)) {
          if (table[nt].count(term) && table[nt][term] != p) conflicts.push_back("Conflict at M[" + nt + ", " + term + "]");
          table[nt][term] = p;
        }
      }
    }
  }
  return {table, conflicts};
}

GrammarArtifacts analyzeCFG(const CFGDefinition& cfg) {
  GrammarTransformer leftRec(cfg.productions);
  auto noLeftRec = leftRec.removeLeftRecursion();
  GrammarTransformer factoredTransformer(noLeftRec);
  auto factored = factoredTransformer.leftFactor();

  FirstFollowAnalyzer analyzer(factored, cfg.startSymbol);
  auto first = analyzer.computeFirst();
  auto follow = analyzer.computeFollow(first);
  auto [parseTable, conflicts] = analyzer.buildParseTable(first, follow);
  auto ll1Validation = validateLL1(factored, first, follow);
  return GrammarArtifacts{factored, first, follow, parseTable, conflicts, ll1Validation, {}};
}

LL1PanicParseResult parseLL1WithPanicMode(const std::vector<Token>& tokens,
                                          const std::string& startSymbol,
                                          const GrammarMap& grammar,
                                          const SetMap& follow,
                                          const ParseTable& parseTable) {
  LL1PanicParseResult result;
  if (tokens.empty()) {
    result.actionLog.push_back("Input token stream is empty.");
    return result;
  }

  std::vector<std::string> stack{"EOF", startSymbol};
  std::size_t index = 0;
  constexpr std::size_t maxSteps = 20000;
  std::size_t steps = 0;

  auto lookahead = [&tokens, &index]() -> std::string {
    if (index >= tokens.size()) return "EOF";
    return tokenGrammarSymbol(tokens[index]);
  };

  while (!stack.empty() && steps++ < maxSteps) {
    const std::string top = stack.back();
    const std::string current = lookahead();

    if (top == EPSILON) {
      stack.pop_back();
      continue;
    }

    if (top == "EOF") {
      if (current == "EOF") {
        result.accepted = (result.errorsRecovered == 0);
        result.actionLog.push_back("ACCEPT");
        return result;
      }
      result.errorsRecovered++;
      result.actionLog.push_back("Panic scan: discard token '" + current + "' while expecting EOF");
      if (index < tokens.size()) {
        ++index;
      } else {
        break;
      }
      continue;
    }

    const bool topIsTerminal = isTerminalSymbol(top, grammar, follow, parseTable);
    if (topIsTerminal) {
      if (top == current) {
        stack.pop_back();
        if (index < tokens.size()) ++index;
        result.actionLog.push_back("Match terminal '" + current + "'");
      } else {
        stack.pop_back();
        result.errorsRecovered++;
        result.actionLog.push_back("Panic pop: terminal mismatch, pop '" + top + "' while seeing '" + current + "'");
      }
      continue;
    }

    auto ntRowIt = parseTable.find(top);
    if (ntRowIt != parseTable.end()) {
      auto prodIt = ntRowIt->second.find(current);
      if (prodIt != ntRowIt->second.end()) {
        const auto production = prodIt->second;
        stack.pop_back();
        for (auto it = production.rbegin(); it != production.rend(); ++it) {
          if (*it != EPSILON) stack.push_back(*it);
        }
        result.actionLog.push_back("Expand " + top + " -> " + productionToString(production));
        continue;
      }
    }

    const auto followIt = follow.find(top);
    const bool inFollow = (followIt != follow.end() && followIt->second.find(current) != followIt->second.end());
    if (inFollow || current == "EOF") {
      stack.pop_back();
      result.errorsRecovered++;
      result.actionLog.push_back("Panic pop: pop non-terminal '" + top + "' on lookahead '" + current + "'");
    } else {
      result.errorsRecovered++;
      result.actionLog.push_back("Panic scan: discard token '" + current + "' while synchronizing for '" + top + "'");
      if (index < tokens.size()) {
        ++index;
      } else {
        break;
      }
    }
  }

  if (steps >= maxSteps) {
    result.actionLog.push_back("Aborted: exceeded parser step limit during panic-mode recovery");
  }
  return result;
}

}  // namespace cd
