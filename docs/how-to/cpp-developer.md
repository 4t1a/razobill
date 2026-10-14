# Embedding librazobill in C++

Build and link the high-performance C++20 engine into native applications and services.
Read this guide if you are writing C or C++ services that need native in-process log decomposition.
Skip this guide if you are calling Razobill through Python or SQL.

---

## Problem

Application binaries embedding external libraries require predictable memory lifetimes and zero runtime overhead.
You need a C++ engine that guarantees zero allocations on the parsing path while remaining safe across ABI boundaries.

---

## Solution

`librazobill` can be embedded directly via CMake or linked as a universal C ABI shared library (`RazobillC.h`).

---

## CMake integration

### Option A: subdirectory integration
Add Razobill to your CMake build tree:

```cmake
cmake_minimum_required(VERSION 3.20)
project(my_telemetry_service LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(path/to/razobill)

add_executable(my_service main.cpp)
target_link_libraries(my_service PRIVATE razobill_core)
```

### Option B: universal C ABI linking
If linking dynamically across binary boundaries, include the public header and link against the shared library:

```c
#include "librazobill/RazobillC.h"
#include <stdio.h>
#include <assert.h>

int main(void) {
    const char message[] = "Host node-1 reported error 404";
    size_t length = sizeof(message) - 1;

    RzRecord record;
    RzStatus status = RzSplit(message, length, &record);
    assert(status == RZ_OK);

    printf("Template: %.*s\n", (int)record.templateLength, record.templateStr);
    printf("Numbers extracted: %zu\n", record.intCount);
    printf("Words extracted:   %zu\n", record.strCount);

    // Free heap memory allocated for record structures
    RzFree(&record);
    return 0;
}
```

---

## Memory lifetime invariants

> [!WARNING]
> **Gotcha**: Pointers in `record.strValues` point directly into the caller's `message` buffer. The source message buffer must remain valid until you are finished reading `record.strValues`.

1. Always call `RzFree(&record)` when done with a record to prevent memory leaks.
2. In C++, prefer `razobill::core::SlotRecord` when working entirely inside C++ code for RAII safety.
