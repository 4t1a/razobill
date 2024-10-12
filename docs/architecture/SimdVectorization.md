# SIMD Vectorization & Hardware Acceleration

To process tens of millions of log lines per second, `librazobill` utilizes SIMD (Single Instruction, Multiple Data) vectorization. Rather than evaluating characters one byte at a time in a scalar loop, `SimdScanner.hpp` inspects 32 bytes simultaneously in hardware AVX2 registers.

---

## 1. Delimiter Scanning Bottleneck

In normative log tokenization, tokens are separated by delimiter characters:
`\t \n \f \r space = , ; : ( ) [ ] { } " ' < > | /`

In scalar execution:
```cpp
// Scalar: Evaluates each byte individually with branch predictor stalls
for (std::size_t i = 0; i < len; ++i) {
    if (isDelimiter(data[i])) { // Branch misprediction on unstructured text
        handleDelimiter(i);
    }
}
```
For an 80-byte log message, scalar scanning requires 80 loop iterations and dozens of branch evaluations, causing pipeline stalls.

---

## 2. AVX2 32-Byte Vectorized Algorithm

[`include/librazobill/SimdScanner.hpp`](../../include/librazobill/SimdScanner.hpp) implements a vectorized scanning algorithm:

```
Input Stream (32 Bytes Loaded into YMM Register):
[ 'F', 'a', 'i', 'l', 'e', 'd', ' ', 'p', 'a', 's', 's', 'w', 'o', 'r', 'd', ' ', ... ]
                              │                                            │
                              ▼                                            ▼
Compare Against Delimiter Vectors (_mm256_cmpeq_epi8):
Vector(Space):     [ 0, 0, 0, 0, 0, 0, 0xFF, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, ... ]
Vector(Colon):     [ 0, 0, 0, 0, 0, 0, 0x00, 0, 0, 0, 0, 0, 0, 0, 0, 0x00, ... ]
Vector(Equals):    [ 0, 0, 0, 0, 0, 0, 0x00, 0, 0, 0, 0, 0, 0, 0, 0, 0x00, ... ]
                              │
                              ▼ Combine via Bitwise OR (_mm256_or_si256)
Match Mask:        [ 0, 0, 0, 0, 0, 0, 0xFF, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, ... ]
                              │
                              ▼ Extract 32-bit Scalar Bitmask (_mm256_movemask_epi8)
Bitmask (uint32_t): 0b00000000000000001000000001000000
                              │
                              ▼ Branchless Bit Scanning (_tzcnt_u64 / __builtin_ctzll)
Offsets Found:     Index 6, Index 15 (Zero branch mispredictions)
```

### Execution Steps
1. **Vector Load**:
   `__m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr));`
2. **Parallel Equality Check**:
   Equality comparisons against pre-broadcast delimiter vectors (`_mm256_set1_epi8(' ')`, `':'`, `'='`, etc.) execute concurrently across execution ports in a single CPU cycle.
3. **Bitmask Extraction**:
   `_mm256_movemask_epi8` gathers the most significant bit of each of the 32 vector bytes into a standard 32-bit unsigned integer bitmask.
4. **Branchless Trailing-Zero Indexing**:
   Instead of testing bit by bit, the next delimiter offset is extracted in a single instruction using hardware trailing-zero count instructions:
   ```cpp
   #if defined(_MSC_VER)
       unsigned long index;
       _BitScanForward64(&index, mask);
   #else
       int index = __builtin_ctzll(mask);
   #endif
   ```
   The processed bit is cleared via `mask &= (mask - 1);`, and scanning continues branchlessly until the mask is zero.

---

## 3. Architecture Support & Dynamic CPUID Multiversioning

`librazobill` features dynamic runtime CPUID feature detection as well as compile-time dispatch to execute the widest available hardware vector register on the host CPU:

| Architecture | Instruction Set | Chunk Size | Acceleration Method |
|---|---|---|---|
| **x86_64 High-End Server** | AVX-512BW + BMI2 | 64 Bytes | `_mm512_cmpeq_epi8_mask` + `_tzcnt_u64` |
| **x86_64 Modern** | AVX2 + BMI2 | 32 Bytes | `_mm256_movemask_epi8` + `_tzcnt_u64` |
| **x86_64 Legacy** | SSE4.2 | 16 Bytes | `_mm_movemask_epi8` + `__builtin_ctz` |
| **ARM64 / Apple Silicon** | NEON | 16 Bytes | `vceqq_u8` + `vshrn_n_u16` + `countTrailingZeros64` |
| **Generic / Embedded** | Standard C++20 | 1 Byte | 256-entry lookup table (`Scanner.hpp`) |

### Dynamic Runtime Dispatch (`findWhitespaceDynamic`)

```text
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                        DYNAMIC CPUID VECTOR KERNEL DISPATCH FLOW                       │
└────────────────────────────────────────────────────────────────────────────────────────┘
                    findWhitespaceDynamic(data, len)
                                   │
                     Is cached dispatcher initialized?
                                ├── Yes ───────────┐
                                │ No               │
                                ▼                  │
                    Inspect CPUID Feature Flags    │
                                │                  │
         ┌──────────────────────┼──────────────────┤
         ▼                      ▼                  ▼
   [AVX-512BW?]             [AVX2?]                │
   ├── Yes ──► 64-Byte      ├── Yes ──► 32-Byte    │
   │           Kernel       │           Kernel     │
   │ No                     │ No                   │
   └───────────┬────────────┘                      │
               ▼                                   ▼
        16-Byte SSE / Scalar Fallback     Cached Function Pointer
        (256-entry direct LUT)            (Direct Indirect Call: ~1 cycle)
```

On x86-64 platforms, binary distributions can use `findWhitespaceDynamic(data, len)` from [`include/librazobill/SimdScanner.hpp`](../../include/librazobill/SimdScanner.hpp). Upon first invocation, `detectSimdArchitecture()` inspects hardware CPUID flags (`__builtin_cpu_supports("avx512bw")`, `__builtin_cpu_supports("avx2")`) and atomically initializes a function pointer to the highest available vector kernel (`findWhitespaceAvx512`, `findWhitespaceAvx2`, or `findWhitespaceScalar`). Subsequent invocations dispatch through the cached function pointer with zero CPUID overhead, delivering up to 32.2 GB/s delimiter scanning throughput.

The generic scalar fallback uses a statically initialized 256-byte class table (`DELIM_TABLE[256]`), ensuring that even on platforms without vector hardware, tokenization proceeds with zero function call overhead and $O(1)$ byte classification.

