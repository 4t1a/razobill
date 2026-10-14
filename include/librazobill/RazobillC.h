/*
 * Razobill Universal C ABI
 *
 * High-performance, zero-allocation log decomposition and byte-exact reconstruction.
 *
 * ARCHITECTURE & LIFETIMES:
 * 1. RzSplit decomposes an input byte slice into a readable template format
 *    with {i} and {s} placeholders, extracted 64-bit integers, and string token views.
 * 2. ZERO-COPY STRINGS: Pointers in outRecord->strs point directly into the caller's
 *    input message buffer. The message buffer MUST outlive outRecord.
 * 3. RECORD LIFECYCLE: Any memory allocated by RzSplit must be freed with
 *    RzFree(outRecord).
 * 4. LOSSLESS INVERTIBILITY: RzJoin(record, ...) restores the exact original
 *    byte sequence byte-for-byte on all valid records.
 * 5. BUFFER SAFETY: RzJoin and RzTemplate write at most bufferCapacity bytes into
 *    the destination buffer without writing any trailing NUL byte. If bufferCapacity is
 *    insufficient, RZ_ERR_BUFFER_TOO_SMALL is returned and *outLength is set to the
 *    exact required capacity in bytes.
 * 6. FFI CONVENIENCE: Foreign language callers (Go, Rust, Python, Zig, C#) can manually
 *    populate an RzRecord (with internalHandle = NULL) to call RzJoin.
 */
#ifndef LIBRAZOBILL_CAPI_H
#define LIBRAZOBILL_CAPI_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) || defined(__CYGWIN__)
#  if defined(LIBRAZOBILL_EXPORTS)
#    define RZ_API __declspec(dllexport)
#  else
#    define RZ_API
#  endif
#else
#  define RZ_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Result status codes returned by Razobill C functions.
 */
typedef enum RzStatus {
    RZ_OK = 0,
    RZ_ERR_INVALID_ARGUMENT = 1,
    RZ_ERR_BUFFER_TOO_SMALL = 2,
    RZ_ERR_MALFORMED_TEMPLATE = 3,
    RZ_ERR_COUNT_MISMATCH = 4,
    RZ_ERR_MESSAGE_TOO_LARGE = 5,
    RZ_ERR_OUT_OF_MEMORY = 6,
    RZ_ERR_INTERNAL = 7
} RzStatus;

/**
 * Container representing a decomposed log message.
 */
typedef struct RzRecord {
    const char* templateStr;
    size_t templateLength;
    const int64_t* intValues;
    size_t intCount;
    const char* const* strValues;
    const size_t* strLengths;
    size_t strCount;
    void* internalHandle;
} RzRecord;

/**
 * Splits a log message into a template string, 64-bit integers, and variable string slices.
 *
 * @param message Pointer to the input message bytes (does not require NUL termination).
 * @param messageLength Length of the message in bytes.
 * @param outRecord Output record struct populated on success.
 * @return RZ_OK on success, or an error status code.
 */
RZ_API RzStatus RzSplit(
    const char* message,
    size_t messageLength,
    RzRecord* outRecord
);

/**
 * Reconstructs the exact original message from a decomposed record.
 *
 * @param record Pointer to the record to join.
 * @param outBuffer Target buffer to receive the reconstructed bytes (may be NULL if probing capacity).
 * @param bufferCapacity Capacity of outBuffer in bytes.
 * @param outLength Pointer to receive the actual written or required length in bytes (may be NULL).
 * @return RZ_OK on success, or an error status code.
 */
RZ_API RzStatus RzJoin(
    const RzRecord* record,
    char* outBuffer,
    size_t bufferCapacity,
    size_t* outLength
);

/**
 * Extracts only the template representation of a message without allocating parsed arrays.
 *
 * @param message Pointer to the input message bytes.
 * @param messageLength Length of the message in bytes.
 * @param outBuffer Target buffer to receive the template (may be NULL if probing capacity).
 * @param bufferCapacity Capacity of outBuffer in bytes.
 * @param outLength Pointer to receive the actual written or required length in bytes (may be NULL).
 * @return RZ_OK on success, or an error status code.
 */
RZ_API RzStatus RzTemplate(
    const char* message,
    size_t messageLength,
    char* outBuffer,
    size_t bufferCapacity,
    size_t* outLength
);

/**
 * Releases heap memory allocated for a record by RzSplit.
 * Safe to call with NULL or on zeroed records.
 */
RZ_API void RzFree(RzRecord* record);

/**
 * Returns a static human-readable description for a status code.
 */
RZ_API const char* RzStatusMessage(RzStatus status);

/**
 * Returns the runtime library semantic version string (e.g. "2.0.0").
 */
RZ_API const char* RzVersion(void);

#ifdef __cplusplus
}
#endif

#endif /* LIBRAZOBILL_CAPI_H */
