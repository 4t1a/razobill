# C++ Developer Guide

This guide explains how to integrate `librazobill` directly into modern C++20 applications, services, and high-performance ingestion pipelines.

---

## 1. CMake Integration

Add `librazobill` as a subdirectory in your project's `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.20)
project(LogIngester LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

# Add librazobill
add_subdirectory(path/to/razobill)

add_executable(Ingester main.cpp)
target_link_libraries(Ingester PRIVATE librazobill)
```

---

## 2. Parsing & Lossless Reconstruction

### Basic Token Parsing
To tokenize a log message into constant skeleton and variable slots:

```cpp
#include <iostream>
#include "librazobill/Slots.hpp"

int main() {
    std::string msg = "Failed password for root from 192.168.1.100 port 22 ssh2";

    razobill::core::SlotRecord record;
    razobill::Status status = razobill::core::parse(msg, record, razobill::core::ScanMode::Semantic);

    if (status != razobill::Status::Ok) {
        std::cerr << "Parse error: " << static_cast<int>(status) << "\n";
        return 1;
    }

    std::cout << "Form Key   : " << record.formKey() << "\n";
    std::cout << "Form Hash  : " << record.formKeyHash() << "\n";

    std::cout << "Integer Slots (" << record.ints.size() << "): ";
    for (auto val : record.ints) std::cout << val << " ";
    std::cout << "\n";

    std::cout << "String Slots (" << record.strs.size() << "): ";
    for (auto sv : record.strs) std::cout << "'" << sv << "' ";
    std::cout << "\n";

    // 100% Byte-for-byte lossless reconstruction
    std::string rebuilt;
    std::string key = record.formKey();
    razobill::core::reconstruct(key, record.ints, record.strs, rebuilt);

    std::cout << "Reconstructed: " << rebuilt << "\n";
    return 0;
}
```

---

## 3. High-Throughput Zero-Allocation Batch Parsing

To eliminate heap churn when ingesting millions of messages, reuse thread-local buffers or a `MemoryArena`:

```cpp
#include "librazobill/Slots.hpp"
#include "librazobill/MemoryArena.hpp"

void ingestBatch(const std::vector<std::string>& batch) {
    razobill::MemoryArena arena(4 * 1024 * 1024); // 4 MiB bump arena
    razobill::core::SlotRecord localSlots;
    std::string localKey;

    for (const auto& line : batch) {
        // Parse into reused localSlots (zero per-row allocation)
        if (razobill::core::parse(line, localSlots, razobill::core::ScanMode::Semantic) == razobill::Status::Ok) {
            localSlots.formKey(localKey);
            // Process extracted integers and string views...
        }
    }

    // Reset arena memory in O(1) for next batch iteration
    arena.reset();
}
```

---

## 4. Multi-Core Concurrent Execution (`ThreadPool`)

To process batches across all hardware CPU threads concurrently:

```cpp
#include "librazobill/ThreadPool.hpp"
#include "librazobill/Slots.hpp"

void parallelAudit(const std::vector<std::string>& lines) {
    std::size_t numThreads = std::thread::hardware_concurrency();
    razobill::core::ThreadPool pool(numThreads);

    std::size_t chunkSize = 2000;
    std::atomic<std::size_t> totalMismatches{0};

    pool.parallelFor(lines.size(), chunkSize, [&](std::size_t start, std::size_t end) {
        razobill::core::SlotRecord slots;
        std::string rebuilt;
        std::string key;

        for (std::size_t i = start; i < end; ++i) {
            const auto& line = lines[i];
            if (razobill::core::parse(line, slots) != razobill::Status::Ok) continue;

            slots.formKey(key);
            if (razobill::core::reconstruct(key, slots.ints, slots.strs, rebuilt) != razobill::Status::Ok ||
                rebuilt != line) {
                totalMismatches.fetch_add(1, std::memory_order_relaxed);
            }
        }
    });

    std::cout << "Audit complete. Mismatches: " << totalMismatches.load() << "\n";
}
```
