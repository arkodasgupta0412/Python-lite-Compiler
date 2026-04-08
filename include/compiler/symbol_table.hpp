#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace cd {

struct SymbolRecord {
  std::string name;
  std::string varType;
  std::string valueRepr;
};

class ISymbolTable {
 public:
  virtual ~ISymbolTable() = default;
  virtual void enterScope() = 0;
  virtual void exitScope() = 0;
  virtual void assign(const std::string& name, const std::string& varType, const std::string& valueRepr) = 0;
  virtual bool lookup(const std::string& name, SymbolRecord& outRecord) const = 0;
  virtual std::unordered_map<std::string, SymbolRecord> snapshot() const = 0;
};

class HashSymbolTable final : public ISymbolTable {
 public:
  HashSymbolTable();
  void enterScope() override;
  void exitScope() override;
  void assign(const std::string& name, const std::string& varType, const std::string& valueRepr) override;
  bool lookup(const std::string& name, SymbolRecord& outRecord) const override;
  std::unordered_map<std::string, SymbolRecord> snapshot() const override;

 private:
  std::vector<std::unordered_map<std::string, SymbolRecord>> scopes_;
};

using SymbolTablePtr = std::unique_ptr<ISymbolTable>;

}  // namespace cd
