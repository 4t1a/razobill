# 03 — Concurrency & Thread Safety Audit

## Scope

ThreadPool.hpp, MemoryArena.hpp, StringPool.hpp, FormCatalog.hpp, RazobillEngine mutex patterns

---

## ThreadPool.hpp

### Findings

#### 🟠 F-POOL-01: `parallelFor` captures `fn` by reference — dangling if `fn` is a temporary

**File**: [`ThreadPool.hpp`](../include/librazobill/ThreadPool.hpp#L113-L152)

```cpp
template <typename F> void parallelFor(std::size_t total, std::size_t chunkSize, F&& fn) {
    // ...
    futures.push_back(submit([&fn, start, end]() { fn(start, end); }));
```

The lambda captures `fn` by reference. If `fn` is a callable rvalue (e.g., a lambda temporary), it's bound to the universal reference parameter which is on the stack. Since `parallelFor` waits for all futures before returning, the reference is valid. **Safe** — but fragile. If any future code path returned early (before `fut.get()`), workers would dereference a dangling reference.

**Severity**: 🟡 MEDIUM — currently safe but fragile
**Fix**: Capture `fn` by value or use `std::ref` explicitly.

---

#### 🟠 F-POOL-02: Exception in `submit()` after partial chunk dispatch

**File**: [`ThreadPool.hpp`](../include/librazobill/ThreadPool.hpp#L129-L137)

```cpp
try {
    for (std::size_t chunkIdx = 0; chunkIdx < numChunks; ++chunkIdx) {
        futures.push_back(submit([&fn, start, end]() { fn(start, end); }));
    }
} catch (...) {
    firstException = std::current_exception();
}
```

If `submit()` throws (e.g., pool is stopping, or `std::bad_alloc`), some chunks are dispatched and some are not. The already-dispatched futures are joined correctly. But the total work is incomplete — caller has no way to know which chunks ran. The function then rethrows.

**Severity**: 🟡 MEDIUM — partial work completion on exception, but exception propagation is correct
**Fix**: Document that `parallelFor` provides at-most-once execution per chunk on exception.

---

#### 🟢 F-POOL-03: `stopping_` is not `std::atomic<bool>`

**File**: [`ThreadPool.hpp`](../include/librazobill/ThreadPool.hpp#L161)

`stopping_` is a plain `bool` always accessed under `mutex_` lock. This is correct — the mutex provides happens-before. Using `std::atomic` would be micro-optimization only.

**Severity**: ⚪ NOTE — correct as-is.

---

## MemoryArena.hpp

### Findings

#### 🔴 F-ARENA-01: Move constructor does not reset `defaultBlockSize_` of moved-from object

**File**: [`MemoryArena.hpp`](../include/librazobill/MemoryArena.hpp#L31-L36)

```cpp
MemoryArena(MemoryArena&& other) noexcept
    : blocks_(std::move(other.blocks_)), currentBlockIdx_(other.currentBlockIdx_),
      offset_(other.offset_), defaultBlockSize_(other.defaultBlockSize_) {
    other.currentBlockIdx_ = 0;
    other.offset_ = 0;
    // other.defaultBlockSize_ is NOT reset — but blocks_ is moved out, so new allocations
    // by the moved-from object will work correctly with the original defaultBlockSize_.
}
```

After move, `other.blocks_` is empty (moved), so `other` is a valid empty arena. The non-reset `defaultBlockSize_` is harmless. **Not a bug**, but:

**Severity**: 🟢 LOW — moved-from state is valid but unconventional
**Fix**: Set `other.defaultBlockSize_ = 0` for clarity (prevents accidental allocation in moved-from arena).

---

#### 🟡 F-ARENA-02: `allocate()` slow path increments `currentBlockIdx_` beyond bounds

**File**: [`MemoryArena.hpp`](../include/librazobill/MemoryArena.hpp#L70-L81)

```cpp
while (++currentBlockIdx_ < blocks_.size()) {
```

If all existing blocks are too small, `currentBlockIdx_` ends at `blocks_.size()`. Then the new block is appended and `currentBlockIdx_ = blocks_.size() - 1`. **Correct** — but the increment modifies state before the size comparison, which is valid C++ but could cause confusion under debugging.

**Severity**: ⚪ NOTE — correct, no action needed.

---

#### 🟡 F-ARENA-03: No thread safety — arena is single-threaded by design

The arena has no mutex. It's designed as a per-thread scratch allocator. If shared across threads, data races occur on `offset_`, `currentBlockIdx_`, and `blocks_`.

**Severity**: ⚪ NOTE — correct design, should be documented as non-thread-safe.

---

## StringPool.hpp

### Findings

#### 🟡 F-SPOOL-01: Double-checked locking pattern on `intern()` is correct

```cpp
std::uint32_t intern(std::string_view s) {
    { std::shared_lock lock(mutex_); /* check */ }
    std::unique_lock lock(mutex_);   /* check again, insert */
}
```

Classic DCLP with `shared_mutex`. The shared→exclusive lock upgrade is done by releasing the shared lock and acquiring exclusive. This leaves a window where another thread could insert the same string. The second check inside the exclusive lock handles this. **Correct**.

**Severity**: ⚪ NOTE — verified correct.

---

#### 🟡 F-SPOOL-02: `lookup()` returns `string_view` to internal storage — invalidated by `gc()`

**File**: [`StringPool.hpp`](../include/librazobill/StringPool.hpp#L72-L78)

```cpp
[[nodiscard]] std::string_view lookup(std::uint32_t id) const {
    std::shared_lock lock(mutex_);
    if (id < idToStr_.size()) return idToStr_[id];
}
```

The returned `string_view` points into `std::deque<std::string>`. If `gc()` is called concurrently (taking exclusive lock), the `deque` is replaced and the view becomes dangling. The `shared_lock` is released when `lookup()` returns, so the caller holds a view without a lock.

**Severity**: 🟠 HIGH — use-after-free if `gc()` is called while views from `lookup()` are live
**Fix**: Document that callers must not hold `lookup()` results across `gc()` calls, or use `lookupCopy()` / `withString()` instead.

---

## FormCatalog.hpp

### Findings

#### 🟡 F-FCAT-01: `formsById_` stores raw pointers to `formsByKey_` entries

**File**: [`FormCatalog.hpp`](../include/librazobill/FormCatalog.hpp#L115-L116)

```cpp
auto inserted = formsByKey_.emplace(keyStr, std::move(entry));
formsById_.push_back(&inserted.first->second);
```

`std::unordered_map` guarantees that references/pointers to elements are not invalidated by insertion. **Correct** — but a rehash or `clear()` invalidates all pointers. The `clear()` method clears all three containers together, so pointers don't dangle. **Safe**.

**Severity**: ⚪ NOTE — correct, relies on `unordered_map` iterator stability guarantee.

---

#### 🟢 F-FCAT-02: `collisionCount_` accessed under shared lock in `collisionCount()`

```cpp
[[nodiscard]] std::size_t collisionCount() const noexcept {
    std::shared_lock lock(mutex_);
    return collisionCount_;
}
```

`collisionCount_` is modified in `registerFormWithHashGetId()` under exclusive lock. Read under shared lock is correct.

**Severity**: ⚪ NOTE — correct.

---

## RazobillEngine Mutex

### Findings

#### 🟡 F-ENGINE-01: All public methods acquire `mu_` — no concurrent reads possible

**File**: [`Razobill.cpp`](../src/Razobill.cpp)

Every public method uses `std::scoped_lock lock(mu_)`, which is an exclusive lock. `version()`, `meaningOf()`, `todo()`, `ledger()`, `leakage()` are all read-only operations but still acquire exclusive access. Under heavy read load, this serializes all calls.

**Severity**: 🟡 MEDIUM — performance under concurrent reads
**Fix**: Replace `std::mutex mu_` with `std::shared_mutex` and use `shared_lock` for read-only methods.

---

#### ⚪ F-ENGINE-02: Lock is not held across database calls

All `Locked` methods hold `mu_` while calling `db_.exec()` and `db_.query()`. The ADBC backend has its own `connMutex_` inside `run()`. This means the engine lock and the connection lock are nested: `mu_` → `connMutex_`. Lock ordering is consistent (always engine first, then connection), so no deadlock. **Correct**.

**Severity**: ⚪ NOTE — lock ordering is consistent.
