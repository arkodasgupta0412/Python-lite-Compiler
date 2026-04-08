#include "compiler/symbol_table.hpp"

namespace cd {

HashSymbolTable::HashSymbolTable() { scopes_.push_back({}); }

void HashSymbolTable::enterScope() { scopes_.push_back({}); }

void HashSymbolTable::exitScope() {
  if (scopes_.size() <= 1) return;
  scopes_.pop_back();
}

void HashSymbolTable::assign(const std::string& name, const std::string& varType,
                             const std::string& valueRepr) {
  for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
    auto found = it->find(name);
    if (found != it->end()) {
      found->second = SymbolRecord{name, varType, valueRepr};
      return;
    }
  }
  scopes_.back()[name] = SymbolRecord{name, varType, valueRepr};
}

bool HashSymbolTable::lookup(const std::string& name, SymbolRecord& outRecord) const {
  for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
    auto found = it->find(name);
    if (found != it->end()) {
      outRecord = found->second;
      return true;
    }
  }
  return false;
}

std::unordered_map<std::string, SymbolRecord> HashSymbolTable::snapshot() const {
  std::unordered_map<std::string, SymbolRecord> flattened;
  for (const auto& scope : scopes_) {
    for (auto it = scope.begin(); it != scope.end(); ++it) {
      flattened[it->first] = it->second;
    }
  }
  return flattened;
}

}  // namespace cd
