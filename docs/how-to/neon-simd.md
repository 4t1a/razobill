# Hardware acceleration with SIMD

Optimize delimiter tokenization on ARM NEON and x86 AVX architectures.
Read this guide if you are compiling Razobill from source for specialized CPU architectures.
Skip this guide if you are using pre-compiled wheels or community packages.

---

## Problem

Default compiler builds often emit generic x86-64 or ARMv8 scalar code to maintain broad binary compatibility.
This leaves hardware vector registers (AVX2, AVX-512, and ARM NEON) unused on modern servers.

---

## Solution

Razobill includes vectorized delimiter scanners in `SimdScanner.hpp`.
It supports both compile-time vector flags and runtime CPUID multiversioning.

---

## Compiler configuration

### ARM NEON targets

On AArch64 targets, NEON vectorization is enabled by default with standard optimization flags:

```bash
# GCC or Clang on Linux AArch64 / macOS ARM64
cmake -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_FLAGS="-O3 -march=armv8-a+simd"
cmake --build build
```

### x86-64 targets

Enable AVX2 vectorization:

```bash
# GCC or Clang on Linux x86_64
cmake -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_FLAGS="-O3 -mavx2 -mbmi"
cmake --build build
```

For server clusters supporting AVX-512BW:

```bash
cmake -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_FLAGS="-O3 -mavx512bw -mavx512vl"
cmake --build build
```

---

## Verification

Run the microbenchmark executable to verify hardware acceleration:

```bash
./build/BenchCore.exe
```

Look for the reported scanner implementation in the console output:

- `AVX-512BW`: 64 bytes per cycle.
- `AVX2`: 32 bytes per cycle.
- `ARM NEON`: 16 bytes per cycle.
