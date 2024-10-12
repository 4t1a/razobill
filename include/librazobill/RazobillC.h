// Universal C ABI Header for librazobill.
// Provides pure C linkage (extern "C") with zero C++ runtime dependencies for foreign language interop.
//
// ARCHITECTURE: Why a Pure C ABI?
// C++ is powerful, but its ABI is notoriously fragile: name mangling varies between GCC and MSVC,
// std::string layout changes between standard library revisions, and C++ exceptions cannot cross
// language boundaries without crashing the host process.
//
// A pure C ABI is eternal.
// Whether calling from Rust (bindgen), Go (cgo), Python (ctypes/cffi), Zig, C#, or Java (JNI/Panama),
// primitive pointers, size_t lengths, and numeric status codes work seamlessly across all platforms.
//
// OWNERSHIP INVARIANT:
// Any memory allocated inside the C++ runtime must be released via `RzFreeRecord()`.
// Never call `free()` directly on pointers returned by `RzSlotRecord`.
#ifndef LIBRAZOBILL_CAPI_H
#define LIBRAZOBILL_CAPI_H

#include <stdint.h>
#include <stddef.h>

#if defined(_WIN32) || defined(__CYGWIN__)
#if defined(LIBRAZOBILL_EXPORTS)
#define RZ_API __declspec(dllexport)
#else
#define RZ_API
#endif
#else
#define RZ_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum RzStatusCode {
    RZ_SUCCESS = 0,
    RZ_ERR_TOO_LARGE = 1,
    RZ_ERR_MALFORMED = 2,
    RZ_ERR_SLOT_UNDERFLOW = 3,
    RZ_ERR_BUFFER_TOO_SMALL = 4,
    RZ_ERR_INVALID_PARAM = 5
} RzStatusCode;

typedef enum RzScanMode { RZ_SCAN_EXACT = 0, RZ_SCAN_SEMANTIC = 1 } RzScanMode;

typedef struct RzSlotRecord {
    uint64_t formKeyHash;
    const char* formKey;
    size_t formKeyLength;
    const int64_t* intValues;
    size_t intCount;
    const char* const* strValues;
    const size_t* strLengths;
    size_t strCount;
    void* internalHandle;
} RzSlotRecord;

// Opaque engine handle
typedef struct RzEngine RzEngine;

// Lifecycle
RZ_API RzEngine* RzCreateEngine(void);
RZ_API void RzDestroyEngine(RzEngine* engine);

// Parsing & Lossless Reconstruction
RZ_API RzStatusCode RzParse(const char* message, size_t messageLength, RzScanMode mode, RzSlotRecord* outRecord);
RZ_API RzStatusCode RzDecompose(const char* message, size_t messageLength, RzScanMode mode, RzSlotRecord* outRecord);
RZ_API RzStatusCode RzReconstruct(const RzSlotRecord* record, char* outBuffer, size_t bufferCapacity,
                                  size_t* outLength);
RZ_API RzStatusCode RzReconstructSlots(const char* formKey, size_t formKeyLength, const int64_t* intValues,
                                       size_t intCount, const char* const* strValues, const size_t* strLengths,
                                       size_t strCount, char* outBuffer, size_t bufferCapacity, size_t* outLength);
RZ_API void RzFreeRecord(RzSlotRecord* record);

// Utility functions
RZ_API RzStatusCode RzTemplateOf(const char* formKey, size_t formKeyLength, char* outBuffer, size_t bufferCapacity,
                                 size_t* outLength);
RZ_API uint64_t RzHash64(const char* text, size_t length);

// Theoretical Ledger & Information Theory C API (Latitude and Doubt, 2026)
typedef struct RzCell {
    uint64_t form;
    uint32_t meaning;
    double mass;
} RzCell;

typedef struct RzLedger {
    double coverage;
    double content;
    double expression;
    double latitude;
    double doubt;
    double conveyance;
    double distance;
    double effectiveVocabulary;
    double effectiveContent;
    double effectiveLatitude;
    double effectiveDoubt;
    double contentMm;
    double expressionMm;
    double latitudeMm;
    double doubtMm;
    double conveyanceMm;
    double contentEpsilon;
    double contentHalfWidth;
} RzLedger;

typedef struct RzCertifiedInterval {
    double epsilon;
    double halfWidth;
} RzCertifiedInterval;

// Theorem 7: The Price of Saying (Section 4.1)
typedef struct RzPriceOfSaying {
    double expectedMeaningBits;
    double expectedExpressionBits;
    double expectedClarificationBits;
    double totalSayingBits;
    double sayingCostBits;
    double lowerBoundBits;
    double upperBoundBits;
} RzPriceOfSaying;

RZ_API RzStatusCode RzComputeLedger(const RzCell* cells, size_t cellCount, double totalEvents, double confidenceAlpha,
                                    RzLedger* outLedger);
RZ_API double RzMillerMadow(double pluginEntropy, size_t observedCategories, double sampleSize);
RZ_API RzCertifiedInterval RzComputeCertifiedInterval(size_t alphabetSize, double sampleSize, double confidenceAlpha);
RZ_API RzStatusCode RzComputePriceOfSaying(const RzLedger* ledger, double nMessages, RzPriceOfSaying* outPrice);

// Version & ABI metadata
RZ_API const char* RzVersion(void);
RZ_API uint32_t RzVersionMajor(void);
RZ_API uint32_t RzVersionMinor(void);
RZ_API uint32_t RzVersionPatch(void);
RZ_API uint32_t RzAbiVersion(void);

#ifdef __cplusplus
}
#endif

#endif // LIBRAZOBILL_CAPI_H
