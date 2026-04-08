#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace cd {

extern const std::string EPSILON;

struct CFGDefinition {
  std::string startSymbol;
  std::unordered_map<std::string, std::vector<std::vector<std::string>>> productions;
};

class CFGProvider {
 public:
  static CFGDefinition rawCFG();
};

}  // namespace cd
