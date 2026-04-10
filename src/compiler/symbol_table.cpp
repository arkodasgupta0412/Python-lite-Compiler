#include "compiler/symbol_table.hpp"

#include <algorithm>
#include <cstring>
#include <mutex>
#include <new>
#include <sstream>

namespace {
constexpr double kMaxLoadFactor = 0.70;
constexpr std::size_t kMinCapacity = 16;

std::size_t nextPowerOfTwo(std::size_t n) {
  std::size_t p = 1;
  while (p < n) p <<= 1;
  return std::max<std::size_t>(p, kMinCapacity);
}

std::size_t pointerHash(std::size_t value) {
  value >>= 3;
  return value * 11400714819323198485ull;
}

int alignTo(int value, int alignment) {
  if (alignment <= 1) return value;
  const int remainder = value % alignment;
  return remainder == 0 ? value : (value + (alignment - remainder));
}

bool needsStorage(cd::SymbolKind kind) {
  return kind == cd::SymbolKind::Variable || kind == cd::SymbolKind::Parameter;
}

int typeStorageSize(const cd::Type* type) {
  if (type == nullptr) return 8;

  switch (type->tag()) {
    case cd::TypeTag::Primitive: {
      const auto* prim = static_cast<const cd::PrimitiveType*>(type);
      switch (prim->kind()) {
        case cd::PrimitiveKind::Int:
          return 4;
        case cd::PrimitiveKind::Float:
          return 8;
        case cd::PrimitiveKind::Bool:
          return 1;
        case cd::PrimitiveKind::Str:
          return 8;
      }
      break;
    }
    case cd::TypeTag::List:
      return 8;
    case cd::TypeTag::Function:
      return 8;
    case cd::TypeTag::Error:
      return 4;
  }

  return 8;
}

}

namespace cd {

ArenaAllocator::ArenaAllocator(std::size_t chunkBytes) : chunkBytes_(std::max<std::size_t>(chunkBytes, 4096)) {
  // Slide 1: Pre-grab large chunks so hot allocations stay O(1) bump-pointer ops.
  addChunk(chunkBytes_);
}

void ArenaAllocator::addChunk(std::size_t minBytes) {
  const std::size_t cap = std::max(chunkBytes_, minBytes);
  Chunk chunk;
  chunk.data = std::make_unique<std::byte[]>(cap);
  chunk.used = 0;
  chunk.capacity = cap;
  chunks_.push_back(std::move(chunk));
}

void* ArenaAllocator::allocate(std::size_t bytes, std::size_t alignment) {
  if (bytes == 0) return nullptr;
  if (alignment == 0) alignment = alignof(std::max_align_t);

  Chunk* chunk = &chunks_.back();
  auto current = reinterpret_cast<std::uintptr_t>(chunk->data.get() + chunk->used);
  std::size_t misalignment = current % alignment;
  std::size_t adjustment = (misalignment == 0) ? 0 : (alignment - misalignment);

  if (chunk->used + adjustment + bytes > chunk->capacity) {
    addChunk(bytes + alignment);
    chunk = &chunks_.back();
    current = reinterpret_cast<std::uintptr_t>(chunk->data.get() + chunk->used);
    misalignment = current % alignment;
    adjustment = (misalignment == 0) ? 0 : (alignment - misalignment);
  }

  std::size_t offset = chunk->used + adjustment;
  // Slide 1: Allocation is just pointer bumping inside arena chunks.
  chunk->used = offset + bytes;
  return chunk->data.get() + offset;
}

StringPool::StringPool(ArenaAllocator& arena, std::size_t initialCapacity)
    : arena_(arena), table_(nextPowerOfTwo(initialCapacity)) {}

std::size_t StringPool::hashString(std::string_view text) noexcept {
  std::size_t h = 1469598103934665603ull;
  for (char c : text) {
    h ^= static_cast<unsigned char>(c);
    h *= 1099511628211ull;
  }
  return h;
}

std::size_t StringPool::findSlotLocked(std::string_view raw) const noexcept {
  const std::size_t mask = table_.size() - 1;
  std::size_t idx = hashString(raw) & mask;
  while (table_[idx].occupied && *table_[idx].value != raw) {
    idx = (idx + 1) & mask;
  }
  return idx;
}

const std::string* StringPool::findExistingLocked(std::string_view raw) const noexcept {
  std::size_t idx = findSlotLocked(raw);
  if (table_[idx].occupied) return table_[idx].value;
  return nullptr;
}

void StringPool::ensureCapacityLocked() {
  if (table_.empty() || static_cast<double>(count_ + 1) / static_cast<double>(table_.size()) > kMaxLoadFactor) {
    rehashLocked(nextPowerOfTwo(std::max<std::size_t>(kMinCapacity, table_.size() * 2)));
  }
}

void StringPool::rehashLocked(std::size_t newCapacity) {
  std::vector<Entry> newTable(nextPowerOfTwo(newCapacity));
  const std::size_t mask = newTable.size() - 1;

  for (const auto& entry : table_) {
    if (!entry.occupied) continue;
    std::size_t idx = hashString(*entry.value) & mask;
    while (newTable[idx].occupied) {
      idx = (idx + 1) & mask;
    }
    newTable[idx] = entry;
  }

  table_.swap(newTable);
}

const std::string* StringPool::intern(std::string_view raw) {
  {
    std::shared_lock<std::shared_mutex> readLock(mutex_);
    if (const std::string* found = findExistingLocked(raw)) return found;
  }

  std::unique_lock<std::shared_mutex> writeLock(mutex_);
  if (const std::string* found = findExistingLocked(raw)) return found;

  // Slide 1: Identifier interning stores one canonical pointer per distinct spelling.
  ensureCapacityLocked();
  std::size_t idx = findSlotLocked(raw);

  char* rawChars = static_cast<char*>(arena_.allocate(raw.size() + 1, alignof(char)));
  std::memcpy(rawChars, raw.data(), raw.size());
  rawChars[raw.size()] = '\0';

  std::string* stored = arena_.create<std::string>(rawChars);
  table_[idx].value = stored;
  table_[idx].occupied = true;
  ++count_;
  return stored;
}

std::size_t StringPool::size() const noexcept { return count_; }

std::size_t PrimitiveType::structuralHash() const {
  return static_cast<std::size_t>(TypeTag::Primitive) * 1315423911u ^ static_cast<std::size_t>(kind_);
}

bool PrimitiveType::structuralEquals(const Type& other) const {
  if (other.tag() != TypeTag::Primitive) return false;
  const auto& rhs = static_cast<const PrimitiveType&>(other);
  return kind_ == rhs.kind_;
}

std::string PrimitiveType::debugName() const {
  switch (kind_) {
    case PrimitiveKind::Int:
      return "int";
    case PrimitiveKind::Float:
      return "float";
    case PrimitiveKind::Bool:
      return "bool";
    case PrimitiveKind::Str:
      return "str";
  }
  return "unknown";
}

std::size_t ListType::structuralHash() const {
  std::size_t elemHash = pointerHash(reinterpret_cast<std::size_t>(elementType_));
  return (static_cast<std::size_t>(TypeTag::List) * 2654435761u) ^ elemHash;
}

bool ListType::structuralEquals(const Type& other) const {
  if (other.tag() != TypeTag::List) return false;
  const auto& rhs = static_cast<const ListType&>(other);
  return elementType_ == rhs.elementType_;
}

std::string ListType::debugName() const { return "list[" + elementType_->debugName() + "]"; }

std::size_t FunctionType::structuralHash() const {
  std::size_t h = static_cast<std::size_t>(TypeTag::Function) * 11400714819323198485ull;
  h ^= pointerHash(reinterpret_cast<std::size_t>(returnType_));
  for (const Type* param : parameterTypes_) {
    h ^= pointerHash(reinterpret_cast<std::size_t>(param)) + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
  }
  return h;
}

bool FunctionType::structuralEquals(const Type& other) const {
  if (other.tag() != TypeTag::Function) return false;
  const auto& rhs = static_cast<const FunctionType&>(other);
  if (returnType_ != rhs.returnType_) return false;
  if (parameterTypes_.size() != rhs.parameterTypes_.size()) return false;
  for (std::size_t i = 0; i < parameterTypes_.size(); ++i) {
    if (parameterTypes_[i] != rhs.parameterTypes_[i]) return false;
  }
  return true;
}

std::string FunctionType::debugName() const {
  std::ostringstream out;
  out << "fn(";
  for (std::size_t i = 0; i < parameterTypes_.size(); ++i) {
    out << parameterTypes_[i]->debugName();
    if (i + 1 < parameterTypes_.size()) out << ", ";
  }
  out << ") -> " << returnType_->debugName();
  return out.str();
}

std::size_t ErrorType::structuralHash() const { return static_cast<std::size_t>(TypeTag::Error) * 16777619u; }

bool ErrorType::structuralEquals(const Type& other) const { return other.tag() == TypeTag::Error; }

std::string ErrorType::debugName() const { return "<error>"; }

TypePool::TypePool(ArenaAllocator& arena, std::size_t initialCapacity)
    : arena_(arena), table_(nextPowerOfTwo(initialCapacity)) {}

std::size_t TypePool::mixHash(std::size_t h) noexcept {
  h ^= (h >> 33);
  h *= 0xff51afd7ed558ccdull;
  h ^= (h >> 33);
  h *= 0xc4ceb9fe1a85ec53ull;
  h ^= (h >> 33);
  return h;
}

std::size_t TypePool::combineHash(std::size_t a, std::size_t b) noexcept {
  return a ^ (b + 0x9e3779b97f4a7c15ull + (a << 6) + (a >> 2));
}

void TypePool::ensureCapacityLocked() {
  if (table_.empty() || static_cast<double>(count_ + 1) / static_cast<double>(table_.size()) > kMaxLoadFactor) {
    rehashLocked(nextPowerOfTwo(std::max<std::size_t>(kMinCapacity, table_.size() * 2)));
  }
}

void TypePool::rehashLocked(std::size_t newCapacity) {
  std::vector<Entry> newTable(nextPowerOfTwo(newCapacity));
  const std::size_t mask = newTable.size() - 1;
  for (const auto& entry : table_) {
    if (!entry.occupied) continue;
    std::size_t idx = mixHash(entry.hash) & mask;
    while (newTable[idx].occupied) {
      idx = (idx + 1) & mask;
    }
    newTable[idx] = entry;
  }
  table_.swap(newTable);
}

std::size_t TypePool::findSlotLocked(std::size_t hash, const Type& candidate) const noexcept {
  const std::size_t mask = table_.size() - 1;
  std::size_t idx = mixHash(hash) & mask;
  while (table_[idx].occupied) {
    if (table_[idx].hash == hash && table_[idx].type->structuralEquals(candidate)) {
      break;
    }
    idx = (idx + 1) & mask;
  }
  return idx;
}

const Type* TypePool::internTypeLocked(Type* candidate) {
  ensureCapacityLocked();
  const std::size_t hash = candidate->structuralHash();
  std::size_t idx = findSlotLocked(hash, *candidate);
  // Slide 1: Structural hashing + interning canonicalizes complex types for pointer-equality checks.
  if (table_[idx].occupied) return table_[idx].type;

  table_[idx].type = candidate;
  table_[idx].hash = hash;
  table_[idx].occupied = true;
  ++count_;
  return candidate;
}

const Type* TypePool::getPrimitive(PrimitiveKind kind) {
  {
    std::shared_lock<std::shared_mutex> readLock(mutex_);
    switch (kind) {
      case PrimitiveKind::Int:
        if (intType_ != nullptr) return intType_;
        break;
      case PrimitiveKind::Float:
        if (floatType_ != nullptr) return floatType_;
        break;
      case PrimitiveKind::Bool:
        if (boolType_ != nullptr) return boolType_;
        break;
      case PrimitiveKind::Str:
        if (strType_ != nullptr) return strType_;
        break;
    }
  }

  std::unique_lock<std::shared_mutex> writeLock(mutex_);
  PrimitiveType* candidate = arena_.create<PrimitiveType>(kind);
  const Type* interned = internTypeLocked(candidate);
  const auto* primitive = static_cast<const PrimitiveType*>(interned);

  switch (kind) {
    case PrimitiveKind::Int:
      intType_ = primitive;
      break;
    case PrimitiveKind::Float:
      floatType_ = primitive;
      break;
    case PrimitiveKind::Bool:
      boolType_ = primitive;
      break;
    case PrimitiveKind::Str:
      strType_ = primitive;
      break;
  }
  return primitive;
}

const Type* TypePool::getList(const Type* elementType) {
  std::unique_lock<std::shared_mutex> writeLock(mutex_);
  ListType* candidate = arena_.create<ListType>(elementType);
  return internTypeLocked(candidate);
}

const Type* TypePool::getFunction(const Type* returnType, const std::vector<const Type*>& parameterTypes) {
  std::unique_lock<std::shared_mutex> writeLock(mutex_);
  FunctionType* candidate = arena_.create<FunctionType>(returnType, parameterTypes);
  return internTypeLocked(candidate);
}

const Type* TypePool::getErrorType() {
  {
    std::shared_lock<std::shared_mutex> readLock(mutex_);
    if (errorType_ != nullptr) return errorType_;
  }

  std::unique_lock<std::shared_mutex> writeLock(mutex_);
  if (errorType_ != nullptr) return errorType_;
  ErrorType* candidate = arena_.create<ErrorType>();
  errorType_ = static_cast<const ErrorType*>(internTypeLocked(candidate));
  return errorType_;
}

FlatSymbolMap::FlatSymbolMap(std::size_t initialCapacity) : table_(nextPowerOfTwo(initialCapacity)) {}

std::size_t FlatSymbolMap::hashKey(const std::string* key) noexcept {
  std::size_t raw = reinterpret_cast<std::size_t>(key);
  return pointerHash(raw);
}

void FlatSymbolMap::ensureCapacity() {
  if (table_.empty() || static_cast<double>(count_ + 1) / static_cast<double>(table_.size()) > kMaxLoadFactor) {
    rehash(nextPowerOfTwo(std::max<std::size_t>(kMinCapacity, table_.size() * 2)));
  }
}

void FlatSymbolMap::rehash(std::size_t newCapacity) {
  std::vector<Slot> newTable(nextPowerOfTwo(newCapacity));
  const std::size_t mask = newTable.size() - 1;

  for (const auto& slot : table_) {
    if (!slot.occupied) continue;
    std::size_t idx = hashKey(slot.key) & mask;
    while (newTable[idx].occupied) {
      idx = (idx + 1) & mask;
    }
    newTable[idx] = slot;
  }

  table_.swap(newTable);
}

bool FlatSymbolMap::define(const std::string* key, const Symbol& value) {
  // Slide 1: Flat linear probing keeps metadata in contiguous memory for cache-friendly lookups.
  ensureCapacity();
  const std::size_t mask = table_.size() - 1;
  std::size_t idx = hashKey(key) & mask;
  while (table_[idx].occupied) {
    if (table_[idx].key == key) return false;
    idx = (idx + 1) & mask;
  }

  table_[idx].key = key;
  table_[idx].value = value;
  table_[idx].occupied = true;
  ++count_;
  return true;
}

Symbol* FlatSymbolMap::find(const std::string* key) {
  if (table_.empty()) return nullptr;
  const std::size_t mask = table_.size() - 1;
  std::size_t idx = hashKey(key) & mask;
  while (table_[idx].occupied) {
    if (table_[idx].key == key) return &table_[idx].value;
    idx = (idx + 1) & mask;
  }
  return nullptr;
}

const Symbol* FlatSymbolMap::find(const std::string* key) const {
  if (table_.empty()) return nullptr;
  const std::size_t mask = table_.size() - 1;
  std::size_t idx = hashKey(key) & mask;
  while (table_[idx].occupied) {
    if (table_[idx].key == key) return &table_[idx].value;
    idx = (idx + 1) & mask;
  }
  return nullptr;
}

std::size_t FlatSymbolMap::size() const noexcept { return count_; }

Scope::Scope(Scope* parent, ArenaAllocator* arena, int initialOffset)
    : parent_(parent), table_(16), arena_(arena), nextOffset_(initialOffset) {}

Scope* Scope::addChild() {
  // Slide 2: Each block gets a child environment that points back to its lexical parent.
  Scope* child = arena_->create<Scope>(this, arena_, nextOffset_);
  children_.push_back(child);
  return child;
}

void Scope::define(const Symbol& symbol) {
  if (!table_.define(symbol.name, symbol)) {
    throw RedefinitionError("Redefinition of symbol '" + *symbol.name + "'");
  }
}

const Symbol* Scope::lookupLocal(const std::string* namePtr) const { return table_.find(namePtr); }

Symbol* Scope::lookupLocalMutable(const std::string* namePtr) { return table_.find(namePtr); }

const Symbol* Scope::lookup(const std::string* namePtr) const {
  // Slide 2: Lexical resolution walks the parent chain until a declaration is found.
  if (const Symbol* local = lookupLocal(namePtr)) return local;
  return (parent_ != nullptr) ? parent_->lookup(namePtr) : nullptr;
}

Symbol* Scope::lookupMutable(const std::string* namePtr) {
  if (Symbol* local = lookupLocalMutable(namePtr)) return local;
  return (parent_ != nullptr) ? parent_->lookupMutable(namePtr) : nullptr;
}

int Scope::allocateOffset(const Type* type, SymbolKind kind) {
  if (!needsStorage(kind)) return -1;

  // Slide 1: Symbol kind + type decide physical stack-frame offset assignment.
  const int size = typeStorageSize(type);
  const int alignment = std::min(size, 8);
  const int assigned = alignTo(nextOffset_, alignment);
  nextOffset_ = assigned + size;
  return assigned;
}

SymbolTableManager::SymbolTableManager()
    : arena_(),
      stringPool_(arena_),
      typePool_(arena_),
      poisonType_(nullptr),
      globalScope_(nullptr),
      currentScope_(nullptr) {
  poisonType_ = typePool_.getErrorType();
  globalScope_ = arena_.create<Scope>(nullptr, &arena_, 0);
  currentScope_ = globalScope_;
}

void SymbolTableManager::enterScope() { currentScope_ = currentScope_->addChild(); }

void SymbolTableManager::exitScope() {
  if (currentScope_->parent() != nullptr) {
    Scope* completed = currentScope_;
    Scope* parent = currentScope_->parent();
    // Slide 2: Flatten child offsets upward so nested scopes share one compact frame layout.
    if (completed->nextOffset() > parent->nextOffset()) {
      parent->setNextOffset(completed->nextOffset());
    }
    // Slide 2: I keep finished scopes alive for diagnostics/LSP snapshots instead of deleting them.
    currentScope_ = parent;
  }
}

void SymbolTableManager::declare(const std::string& rawName,
                                 const Type* type,
                                 SymbolKind kind,
                                 int line,
                                 int column,
                                 int memoryOffset) {
  const std::string* namePtr = stringPool_.intern(rawName);
  int resolvedOffset = memoryOffset;
  if (resolvedOffset < 0) {
    resolvedOffset = currentScope_->allocateOffset(type, kind);
  }
  Symbol symbol{namePtr, type, kind, line, column, resolvedOffset};
  currentScope_->define(symbol);
}

Symbol* SymbolTableManager::findNearestMutable(const std::string* namePtr) {
  for (Scope* scope = currentScope_; scope != nullptr; scope = scope->parent()) {
    if (Symbol* found = scope->lookupLocalMutable(namePtr)) return found;
  }
  return nullptr;
}

void SymbolTableManager::assignOrDeclare(const std::string& rawName,
                                         const Type* type,
                                         SymbolKind kind,
                                         int line,
                                         int column,
                                         int memoryOffset) {
  const std::string* namePtr = stringPool_.intern(rawName);
  if (Symbol* existing = findNearestMutable(namePtr)) {
    existing->type = type;
    existing->kind = kind;
    existing->line = line;
    existing->column = column;
    if (memoryOffset >= 0) {
      existing->memoryOffset = memoryOffset;
    } else if (existing->memoryOffset < 0) {
      existing->memoryOffset = currentScope_->allocateOffset(type, kind);
    }
    return;
  }
  int resolvedOffset = memoryOffset;
  if (resolvedOffset < 0) {
    resolvedOffset = currentScope_->allocateOffset(type, kind);
  }
  Symbol symbol{namePtr, type, kind, line, column, resolvedOffset};
  currentScope_->define(symbol);
}

const Symbol* SymbolTableManager::resolveInterned(const std::string* namePtr,
                                                  int line,
                                                  int column,
                                                  bool poisonOnMiss) const {
  if (const Symbol* found = currentScope_->lookup(namePtr)) return found;

  if (!poisonOnMiss) return nullptr;

  // Slide 3: Report undefined-symbol error once, then move forward without panicking.
  std::ostringstream msg;
  msg << "Undefined symbol '" << *namePtr << "'";
  if (line > 0 && column > 0) {
    msg << " at line " << line << ", column " << column;
  }
  msg << "; inserted poison symbol.";
  diagnostics_.push_back(msg.str());

  if (currentScope_->lookupLocal(namePtr) == nullptr) {
    try {
      // Slide 3: Inject a synthetic <error> variable so downstream analysis can continue safely.
      Symbol poison{namePtr, poisonType_, SymbolKind::Variable, line, column, -1};
      const_cast<Scope*>(currentScope_)->define(poison);
    } catch (const RedefinitionError&) {
    }
  }

  // Slide 3: Returning poison lets later nodes propagate <error> and suppress cascaded noise.
  return currentScope_->lookup(namePtr);
}

const Symbol* SymbolTableManager::resolve(const std::string& name, int line, int column, bool poisonOnMiss) const {
  const std::string* namePtr = stringPool_.intern(name);
  return resolveInterned(namePtr, line, column, poisonOnMiss);
}

const Symbol* SymbolTableManager::lookup(const std::string& name) const {
  const std::string* namePtr = stringPool_.intern(name);
  return currentScope_->lookup(namePtr);
}

const std::string* SymbolTableManager::internString(const std::string& raw) const {
  return stringPool_.intern(raw);
}

const std::string* SymbolTableManager::internStringView(std::string_view raw) const {
  return stringPool_.intern(raw);
}

const Type* SymbolTableManager::intType() const { return typePool_.getPrimitive(PrimitiveKind::Int); }

const Type* SymbolTableManager::floatType() const { return typePool_.getPrimitive(PrimitiveKind::Float); }

const Type* SymbolTableManager::boolType() const { return typePool_.getPrimitive(PrimitiveKind::Bool); }

const Type* SymbolTableManager::strType() const { return typePool_.getPrimitive(PrimitiveKind::Str); }

const Type* SymbolTableManager::listType(const Type* elementType) const { return typePool_.getList(elementType); }

const Type* SymbolTableManager::functionType(const Type* returnType, const std::vector<const Type*>& params) const {
  return typePool_.getFunction(returnType, params);
}

const Type* SymbolTableManager::poisonType() const { return poisonType_; }

void SymbolTableManager::collectScopeSymbols(const Scope* scope, std::vector<Symbol>& out) const {
  scope->forEachLocal([&out](const Symbol& sym) { out.push_back(sym); });
  for (const Scope* child : scope->children()) {
    collectScopeSymbols(child, out);
  }
}

void SymbolTableManager::collectScopeSnapshots(const Scope* scope,
                                               int depth,
                                               int childIndex,
                                               std::vector<ScopeSnapshot>& out) const {
  // Slide 2: Snapshotting retained scopes preserves full lexical history for tooling.
  ScopeSnapshot snapshot;
  snapshot.depth = depth;
  snapshot.childIndex = childIndex;

  scope->forEachLocal([&snapshot](const Symbol& sym) { snapshot.locals.push_back(sym); });
  std::sort(snapshot.locals.begin(), snapshot.locals.end(), [](const Symbol& lhs, const Symbol& rhs) {
    if (lhs.name == nullptr || rhs.name == nullptr) {
      return lhs.name != nullptr;
    }
    if (*lhs.name != *rhs.name) return *lhs.name < *rhs.name;
    if (lhs.line != rhs.line) return lhs.line < rhs.line;
    return lhs.column < rhs.column;
  });

  out.push_back(std::move(snapshot));

  int childCounter = 1;
  for (const Scope* child : scope->children()) {
    collectScopeSnapshots(child, depth + 1, childCounter, out);
    ++childCounter;
  }
}

std::vector<ScopeSnapshot> SymbolTableManager::snapshotScopes() const {
  std::vector<ScopeSnapshot> scopes;
  collectScopeSnapshots(globalScope_, 0, 0, scopes);
  return scopes;
}

std::vector<Symbol> SymbolTableManager::snapshotSymbols() const {
  std::vector<ScopeSnapshot> scopes = snapshotScopes();
  std::size_t totalCount = 0;
  for (const auto& scope : scopes) {
    totalCount += scope.locals.size();
  }

  std::vector<Symbol> flattened;
  flattened.reserve(totalCount);
  for (const auto& scope : scopes) {
    flattened.insert(flattened.end(), scope.locals.begin(), scope.locals.end());
  }

  return flattened;
}

std::vector<std::string> SymbolTableManager::diagnostics() const { return diagnostics_; }

void SymbolTableManager::clearDiagnostics() { diagnostics_.clear(); }

std::string symbolKindName(SymbolKind kind) {
  switch (kind) {
    case SymbolKind::Variable:
      return "Variable";
    case SymbolKind::Function:
      return "Function";
    case SymbolKind::Parameter:
      return "Parameter";
    case SymbolKind::Builtin:
      return "Builtin";
  }
  return "Unknown";
}

std::string typeDebugName(const Type* type) {
  if (type == nullptr) return "<null-type>";
  return type->debugName();
}

}
