#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cd {

class SymbolTableError : public std::runtime_error {
 public:
  explicit SymbolTableError(const std::string& msg) : std::runtime_error(msg) {}
};

class RedefinitionError : public SymbolTableError {
 public:
  explicit RedefinitionError(const std::string& msg) : SymbolTableError(msg) {}
};

// Arena allocator using bump allocation over fixed-size chunks.
class ArenaAllocator {
 public:
  explicit ArenaAllocator(std::size_t chunkBytes = 1U << 20);
  void* allocate(std::size_t bytes, std::size_t alignment);

  template <typename T, typename... Args>
  T* create(Args&&... args) {
    void* mem = allocate(sizeof(T), alignof(T));
    return new (mem) T(std::forward<Args>(args)...);
  }

 private:
  struct Chunk {
    std::unique_ptr<std::byte[]> data;
    std::size_t used{0};
    std::size_t capacity{0};
  };

  std::size_t chunkBytes_;
  std::vector<Chunk> chunks_;

  void addChunk(std::size_t minBytes);
};

class StringPool {
 public:
  explicit StringPool(ArenaAllocator& arena, std::size_t initialCapacity = 128);
  const std::string* intern(std::string_view raw);
  std::size_t size() const noexcept;

 private:
  struct Entry {
    const std::string* value{nullptr};
    bool occupied{false};
  };

  ArenaAllocator& arena_;
  mutable std::shared_mutex mutex_;
  std::vector<Entry> table_;
  std::size_t count_{0};

  static std::size_t hashString(std::string_view text) noexcept;
  std::size_t findSlotLocked(std::string_view raw) const noexcept;
  const std::string* findExistingLocked(std::string_view raw) const noexcept;
  void ensureCapacityLocked();
  void rehashLocked(std::size_t newCapacity);
};

enum class TypeTag { Primitive, List, Function, Error };
enum class PrimitiveKind { Int, Float, Bool, Str };

class Type {
 public:
  virtual ~Type() = default;
  virtual TypeTag tag() const = 0;
  virtual std::size_t structuralHash() const = 0;
  virtual bool structuralEquals(const Type& other) const = 0;
  virtual std::string debugName() const = 0;
};

class PrimitiveType final : public Type {
 public:
  explicit PrimitiveType(PrimitiveKind kind) : kind_(kind) {}
  TypeTag tag() const override { return TypeTag::Primitive; }
  std::size_t structuralHash() const override;
  bool structuralEquals(const Type& other) const override;
  std::string debugName() const override;
  PrimitiveKind kind() const { return kind_; }

 private:
  PrimitiveKind kind_;
};

class ListType final : public Type {
 public:
  explicit ListType(const Type* elementType) : elementType_(elementType) {}
  TypeTag tag() const override { return TypeTag::List; }
  std::size_t structuralHash() const override;
  bool structuralEquals(const Type& other) const override;
  std::string debugName() const override;
  const Type* elementType() const { return elementType_; }

 private:
  const Type* elementType_;
};

class FunctionType final : public Type {
 public:
  FunctionType(const Type* returnType, std::vector<const Type*> parameterTypes)
      : returnType_(returnType), parameterTypes_(std::move(parameterTypes)) {}
  TypeTag tag() const override { return TypeTag::Function; }
  std::size_t structuralHash() const override;
  bool structuralEquals(const Type& other) const override;
  std::string debugName() const override;
  const Type* returnType() const { return returnType_; }
  const std::vector<const Type*>& parameterTypes() const { return parameterTypes_; }

 private:
  const Type* returnType_;
  std::vector<const Type*> parameterTypes_;
};

class ErrorType final : public Type {
 public:
  TypeTag tag() const override { return TypeTag::Error; }
  std::size_t structuralHash() const override;
  bool structuralEquals(const Type& other) const override;
  std::string debugName() const override;
};

class TypePool {
 public:
  explicit TypePool(ArenaAllocator& arena, std::size_t initialCapacity = 128);

  const Type* getPrimitive(PrimitiveKind kind);
  const Type* getList(const Type* elementType);
  const Type* getFunction(const Type* returnType, const std::vector<const Type*>& parameterTypes);
  const Type* getErrorType();

 private:
  struct Entry {
    const Type* type{nullptr};
    std::size_t hash{0};
    bool occupied{false};
  };

  ArenaAllocator& arena_;
  mutable std::shared_mutex mutex_;
  std::vector<Entry> table_;
  std::size_t count_{0};

  const PrimitiveType* intType_{nullptr};
  const PrimitiveType* floatType_{nullptr};
  const PrimitiveType* boolType_{nullptr};
  const PrimitiveType* strType_{nullptr};
  const ErrorType* errorType_{nullptr};

  static std::size_t mixHash(std::size_t h) noexcept;
  static std::size_t combineHash(std::size_t a, std::size_t b) noexcept;
  void ensureCapacityLocked();
  void rehashLocked(std::size_t newCapacity);
  std::size_t findSlotLocked(std::size_t hash, const Type& candidate) const noexcept;
  const Type* internTypeLocked(Type* candidate);
};

enum class SymbolKind {
  Variable,
  Function,
  Parameter,
  Builtin,
};

struct Symbol {
  const std::string* name{nullptr};
  const Type* type{nullptr};
  SymbolKind kind{SymbolKind::Variable};
  int line{0};
  int column{0};
  int memoryOffset{-1};
};

struct ScopeSnapshot {
  int depth{0};
  int childIndex{0};
  std::vector<Symbol> locals;
};

// Open-addressed flat hash map specialized for interned identifier pointers.
class FlatSymbolMap {
 public:
  explicit FlatSymbolMap(std::size_t initialCapacity = 16);

  bool define(const std::string* key, const Symbol& value);
  Symbol* find(const std::string* key);
  const Symbol* find(const std::string* key) const;
  std::size_t size() const noexcept;

  template <typename Fn>
  void forEach(Fn&& fn) const {
    for (const auto& slot : table_) {
      if (slot.occupied) fn(slot.value);
    }
  }

 private:
  struct Slot {
    const std::string* key{nullptr};
    Symbol value{};
    bool occupied{false};
  };

  std::vector<Slot> table_;
  std::size_t count_{0};

  static std::size_t hashKey(const std::string* key) noexcept;
  void ensureCapacity();
  void rehash(std::size_t newCapacity);
};

class Scope {
 public:
  Scope(Scope* parent, ArenaAllocator* arena, int initialOffset = 0);

  Scope* addChild();
  void define(const Symbol& symbol);
  const Symbol* lookup(const std::string* namePtr) const;
  const Symbol* lookupLocal(const std::string* namePtr) const;
  Symbol* lookupMutable(const std::string* namePtr);
  Symbol* lookupLocalMutable(const std::string* namePtr);
  int allocateOffset(const Type* type, SymbolKind kind);

  Scope* parent() const { return parent_; }
  const std::vector<Scope*>& children() const { return children_; }
  int nextOffset() const { return nextOffset_; }
  void setNextOffset(int nextOffset) { nextOffset_ = nextOffset; }

  template <typename Fn>
  void forEachLocal(Fn&& fn) const {
    table_.forEach(std::forward<Fn>(fn));
  }

 private:
  Scope* parent_;
  std::vector<Scope*> children_;
  FlatSymbolMap table_;
  ArenaAllocator* arena_;
  int nextOffset_{0};
};

class SymbolTableManager {
 public:
  SymbolTableManager();

  void enterScope();
  void exitScope();

  void declare(const std::string& rawName,
               const Type* type,
               SymbolKind kind,
               int line,
               int column,
               int memoryOffset = -1);

  // Updates nearest declaration if present; otherwise defines in current scope.
  void assignOrDeclare(const std::string& rawName,
                       const Type* type,
                       SymbolKind kind,
                       int line,
                       int column,
                       int memoryOffset = -1);

  // Lookup with optional poisoning: missing identifiers can be inserted as ErrorType in current scope.
  const Symbol* resolve(const std::string& name, int line = 0, int column = 0, bool poisonOnMiss = true) const;

  // Local/global lookup without poisoning.
  const Symbol* lookup(const std::string& name) const;

  const std::string* internString(const std::string& raw) const;
  const std::string* internStringView(std::string_view raw) const;

  const Type* intType() const;
  const Type* floatType() const;
  const Type* boolType() const;
  const Type* strType() const;
  const Type* listType(const Type* elementType) const;
  const Type* functionType(const Type* returnType, const std::vector<const Type*>& params) const;
  const Type* poisonType() const;

  std::vector<ScopeSnapshot> snapshotScopes() const;
  std::vector<Symbol> snapshotSymbols() const;
  std::vector<std::string> diagnostics() const;
  void clearDiagnostics();

  const Scope* globalScope() const { return globalScope_; }
  const Scope* currentScope() const { return currentScope_; }

 private:
  mutable ArenaAllocator arena_;
  mutable StringPool stringPool_;
  mutable TypePool typePool_;
  const Type* poisonType_;

  Scope* globalScope_;
  mutable Scope* currentScope_;
  mutable std::vector<std::string> diagnostics_;

  const Symbol* resolveInterned(const std::string* namePtr, int line, int column, bool poisonOnMiss) const;
  Symbol* findNearestMutable(const std::string* namePtr);
  void collectScopeSnapshots(const Scope* scope, int depth, int childIndex, std::vector<ScopeSnapshot>& out) const;
  void collectScopeSymbols(const Scope* scope, std::vector<Symbol>& out) const;
};

std::string symbolKindName(SymbolKind kind);
std::string typeDebugName(const Type* type);

}  // namespace cd
