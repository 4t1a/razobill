# Universal C ABI Reference (`RazobillC.h`)

The Universal C ABI provides an `extern "C"` foreign-function interface (FFI) for `librazobill` with zero C++ runtime dependencies. It enables seamless integration from Rust, Go, Zig, C#, Java (JNI), Python (ctypes), and Swift.

Header: [`include/librazobill/RazobillC.h`](../../include/librazobill/RazobillC.h)
Library: `build/liblibrazobill.a` (Static) / `build/librazobill.dll` (Shared)

---

## 1. Status Codes (`RzStatusCode`)

```c
typedef enum RzStatusCode {
    RZ_SUCCESS = 0,             /* Operation completed successfully */
    RZ_ERR_TOO_LARGE = 1,       /* Input exceeds 16 MiB size threshold */
    RZ_ERR_MALFORMED = 2,       /* Form key contains invalid escape bytes */
    RZ_ERR_SLOT_UNDERFLOW = 3,  /* Insufficient slot values provided for holes */
    RZ_ERR_BUFFER_TOO_SMALL = 4,/* Output buffer capacity is insufficient */
    RZ_ERR_INVALID_PARAM = 5    /* Null pointer or invalid argument passed */
} RzStatusCode;
```

---

## 2. Scanning Modes (`RzScanMode`)

```c
typedef enum RzScanMode {
    RZ_SCAN_EXACT = 0,          /* Strict digit-bearing token identification */
    RZ_SCAN_SEMANTIC = 1        /* Identifiers, hostnames, IPs, and usernames */
} RzScanMode;
```

---

## 3. Data Structures (`RzSlotRecord`)

```c
typedef struct RzSlotRecord {
    uint64_t formKeyHash;            /* XXH64 hash of canonical form key */
    const char* formKey;             /* Null-terminated form key string */
    size_t formKeyLength;            /* Length in bytes of form key */
    const int64_t* intValues;        /* Array of extracted 64-bit integers */
    size_t intCount;                 /* Number of integers in array */
    const char* const* strValues;    /* Array of string pointers into message */
    const size_t* strLengths;        /* Array of string lengths in bytes */
    size_t strCount;                 /* Number of strings in array */
    void* internalHandle;            /* Internal memory manager handle */
} RzSlotRecord;
```

### Memory Ownership Contract
- Calling `RzParse(...)` initializes `RzSlotRecord` and allocates memory managed by `internalHandle`.
- The caller **MUST** call `RzFreeRecord(&record)` when finished to avoid memory leaks.
- `strValues` point directly into the underlying message buffer; they remain valid until `RzFreeRecord()` is invoked.

---

## 4. Functions

### `RzParse`
```c
RZ_API RzStatusCode RzParse(
    const char* message,
    size_t messageLength,
    RzScanMode mode,
    RzSlotRecord* outRecord
);
```
Tokenizes a raw log string into structured slots.

---

### `RzDecompose`
```c
RZ_API RzStatusCode RzDecompose(
    const char* message,
    size_t messageLength,
    RzScanMode mode,
    RzSlotRecord* outRecord
);
```
High-speed alias for `RzParse` designed for edge ingestion transforms (e.g. OpenTelemetry Collector, Vector). Extracts slot arrays and canonical form key.

---

### `RzReconstruct`
```c
RZ_API RzStatusCode RzReconstruct(
    const RzSlotRecord* record,
    char* outBuffer,
    size_t bufferCapacity,
    size_t* outLength
);
```
Reassembles the raw message with 100% byte-for-byte exact fidelity into `outBuffer`. If `bufferCapacity` is too small, returns `RZ_ERR_BUFFER_TOO_SMALL` and populates `outLength` with required capacity.

---

### `RzReconstructSlots`
```c
RZ_API RzStatusCode RzReconstructSlots(
    const char* formKey,
    size_t formKeyLength,
    const int64_t* intValues,
    size_t intCount,
    const char* const* strValues,
    const size_t* strLengths,
    size_t strCount,
    char* outBuffer,
    size_t bufferCapacity,
    size_t* outLength
);
```
Symmetrical slot reconstruction without an intermediate `RzSlotRecord`. Enables foreign runtimes (Go, Rust, Zig, C#) to reconstruct directly from primitive arrays.

---

### `RzTemplateOf`
```c
RZ_API RzStatusCode RzTemplateOf(
    const char* formKey,
    size_t formKeyLength,
    char* outBuffer,
    size_t bufferCapacity,
    size_t* outLength
);
```
Converts an internal form key into human-readable `<*>` template text.

---

### `RzHash64`
```c
RZ_API uint64_t RzHash64(const char* text, size_t length);
```
Computes 64-bit XXH64 hash matching ClickHouse `xxHash64`.

---

### `RzFreeRecord`
```c
RZ_API void RzFreeRecord(RzSlotRecord* record);
```
Frees all internal memory associated with a previously parsed `RzSlotRecord`.

---

## 5. Theoretical Ledger & Information Theory C API

Grounded in [*Latitude and Doubt*](https://doi.org/10.5281/zenodo.23287286) (M. M. Atta, 2026; Zenodo DOI: [10.5281/zenodo.23287286](https://doi.org/10.5281/zenodo.23287286)).

### `RzCell`
```c
typedef struct RzCell {
    uint64_t form;       /* Form identifier or hash */
    uint32_t meaning;    /* Meaning / semantic class identifier */
    double mass;         /* Empirical frequency or event count */
} RzCell;
```

### `RzLedger`
```c
typedef struct RzLedger {
    double coverage;             /* Sample coverage */
    double content;              /* Content entropy C = H(W) */
    double expression;           /* Expression entropy E = H(X) */
    double latitude;             /* Slot latitude Λ = H(X | W) */
    double doubt;                /* Semantic doubt Δ = H(W | X) */
    double conveyance;           /* Conveyance I = I(W; X) */
    double distance;             /* Information distance D(W, X) = Λ + Δ */
    double effectiveVocabulary;  /* Effective vocabulary 2^H */
    double effectiveContent;     /* Effective content */
    double effectiveLatitude;    /* Effective latitude */
    double effectiveDoubt;       /* Effective doubt */
    double contentMm;            /* Miller-Madow bias-corrected content */
    double expressionMm;         /* Miller-Madow bias-corrected expression */
    double latitudeMm;           /* Miller-Madow bias-corrected latitude */
    double doubtMm;              /* Miller-Madow bias-corrected doubt */
    double conveyanceMm;         /* Miller-Madow bias-corrected conveyance */
    double contentEpsilon;       /* Proposition 10 epsilon bound */
    double contentHalfWidth;     /* Confidence half-width */
} RzLedger;
```

### `RzCertifiedInterval`
```c
typedef struct RzCertifiedInterval {
    double epsilon;      /* Distribution-free upper bound */
    double halfWidth;    /* Interval radius */
} RzCertifiedInterval;
```

### `RzPriceOfSaying`
```c
typedef struct RzPriceOfSaying {
    double expectedMeaningBits;       /* n * C: meaning transmission bits */
    double expectedExpressionBits;    /* n * E: form transmission bits */
    double expectedClarificationBits; /* n * Delta: clarification questions */
    double totalSayingBits;           /* n * (E + Delta) == n * (C + Lambda) */
    double sayingCostBits;            /* n * Lambda: premium of wording */
    double lowerBoundBits;            /* Shannon lower bound */
    double upperBoundBits;            /* Prefix code upper bound */
} RzPriceOfSaying;
```

### Functions

#### `RzComputeLedger`
```c
RZ_API RzStatusCode RzComputeLedger(
    const RzCell* cells,
    size_t cellCount,
    double totalEvents,
    double confidenceAlpha,
    RzLedger* outLedger
);
```
Computes the full information ledger, verifying $E = I + \Lambda$ and $C = I + \Delta$ along with Miller-Madow corrections and Proposition 10 bounds.

#### `RzComputePriceOfSaying`
```c
RZ_API RzStatusCode RzComputePriceOfSaying(
    const RzLedger* ledger,
    double nMessages,
    RzPriceOfSaying* outPrice
);
```
Computes prefix-code expected lengths and wording premium $n\Lambda$ bits (Theorem 7).

#### `RzMillerMadow`
```c
RZ_API double RzMillerMadow(double pluginEntropy, size_t observedCategories, double sampleSize);
```
Computes the Miller-Madow analytical bias adjustment for finite sample counts.

#### `RzComputeCertifiedInterval`
```c
RZ_API RzCertifiedInterval RzComputeCertifiedInterval(size_t alphabetSize, double sampleSize, double confidenceAlpha);
```
Computes the Proposition 10 distribution-free confidence interval for significance level $\alpha$.

---

## 6. Version Query Functions
```c
RZ_API const char* RzVersion(void);       /* Returns "1.0.0" */
RZ_API uint32_t RzVersionMajor(void);     /* Returns 1 */
RZ_API uint32_t RzVersionMinor(void);     /* Returns 0 */
RZ_API uint32_t RzVersionPatch(void);     /* Returns 0 */
RZ_API uint32_t RzAbiVersion(void);       /* Returns 1 */
```

---

## 7. Complete C Example

```c
#include <stdio.h>
#include <string.h>
#include "librazobill/RazobillC.h"

int main(void) {
    const char* msg = "Failed password for root from 192.168.1.1 port 22";
    size_t len = strlen(msg);

    RzSlotRecord rec;
    if (RzParse(msg, len, RZ_SCAN_SEMANTIC, &rec) == RZ_SUCCESS) {
        printf("Form Key: %s (Hash: %llu)\n", rec.formKey, (unsigned long long)rec.formKeyHash);
        printf("Integers found: %zu, Strings found: %zu\n", rec.intCount, rec.strCount);

        char buffer[256];
        size_t outLen = 0;
        if (RzReconstruct(&rec, buffer, sizeof(buffer), &outLen) == RZ_SUCCESS) {
            buffer[outLen] = '\0';
            printf("Reconstructed: %s\n", buffer);
        }

        RzFreeRecord(&rec);
    }
    return 0;
}
```
