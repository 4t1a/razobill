/*
 * Razobill Universal C ABI
 *
 * High-performance, zero-allocation log decomposition and byte-exact reconstruction.
 *
 * ARCHITECTURE & LIFETIMES:
 * 1. razobill_split decomposes an input byte slice into a readable template format
 *    with {i} and {s} placeholders, extracted 64-bit integers, and string token views.
 * 2. ZERO-COPY STRINGS: Pointers in out_record->strs point directly into the caller's
 *    input message buffer. The message buffer MUST outlive out_record.
 * 3. RECORD LIFECYCLE: Any memory allocated by razobill_split must be freed with
 *    razobill_free(out_record).
 * 4. LOSSLESS INVERTIBILITY: razobill_join(record, ...) restores the exact original
 *    byte sequence byte-for-byte on all valid records.
 * 5. BUFFER SAFETY: razobill_join and razobill_template write at most buf_cap bytes into
 *    the destination buffer without writing any trailing NUL byte. If buf_cap is
 *    insufficient, RAZOBILL_ERR_BUFFER_TOO_SMALL is returned and *out_len is set to the
 *    exact required capacity in bytes.
 * 6. FFI CONVENIENCE: Foreign language callers (Go, Rust, Python, Zig, C#) can manually
 *    populate a razobill_record_t (with internal_handle = NULL) to call razobill_join.
 */
#ifndef RAZOBILL_H
#define RAZOBILL_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) || defined(__CYGWIN__)
#  if defined(RAZOBILL_EXPORTS)
#    define RAZOBILL_API __declspec(dllexport)
#  else
#    define RAZOBILL_API
#  endif
#else
#  define RAZOBILL_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Result status codes returned by Razobill C functions.
 */
typedef enum razobill_status_t {
    RAZOBILL_OK = 0,
    RAZOBILL_ERR_INVALID_ARGUMENT = 1,
    RAZOBILL_ERR_BUFFER_TOO_SMALL = 2,
    RAZOBILL_ERR_MALFORMED_TEMPLATE = 3,
    RAZOBILL_ERR_COUNT_MISMATCH = 4,
    RAZOBILL_ERR_MESSAGE_TOO_LARGE = 5,
    RAZOBILL_ERR_OUT_OF_MEMORY = 6,
    RAZOBILL_ERR_INTERNAL = 7
} razobill_status_t;

/**
 * Container representing a decomposed log message.
 */
typedef struct razobill_record_t {
    const char* template_str;
    size_t template_len;
    const int64_t* ints;
    size_t int_count;
    const char* const* strs;
    const size_t* str_lens;
    size_t str_count;
    void* internal_handle;
} razobill_record_t;

/**
 * Splits a log message into a template string, 64-bit integers, and variable string slices.
 *
 * @param message Pointer to the input message bytes (does not require NUL termination).
 * @param message_len Length of the message in bytes.
 * @param out_record Output record struct populated on success.
 * @return RAZOBILL_OK on success, or an error status code.
 */
RAZOBILL_API razobill_status_t razobill_split(
    const char* message,
    size_t message_len,
    razobill_record_t* out_record
);

/**
 * Reconstructs the exact original message from a decomposed record.
 *
 * @param record Pointer to the record to join.
 * @param out_buf Target buffer to receive the reconstructed bytes (may be NULL if probing capacity).
 * @param buf_cap Capacity of out_buf in bytes.
 * @param out_len Pointer to receive the actual written or required length in bytes (may be NULL).
 * @return RAZOBILL_OK on success, or an error status code.
 */
RAZOBILL_API razobill_status_t razobill_join(
    const razobill_record_t* record,
    char* out_buf,
    size_t buf_cap,
    size_t* out_len
);

/**
 * Extracts only the template representation of a message without allocating parsed arrays.
 *
 * @param message Pointer to the input message bytes.
 * @param message_len Length of the message in bytes.
 * @param out_buf Target buffer to receive the template (may be NULL if probing capacity).
 * @param buf_cap Capacity of out_buf in bytes.
 * @param out_len Pointer to receive the actual written or required length in bytes (may be NULL).
 * @return RAZOBILL_OK on success, or an error status code.
 */
RAZOBILL_API razobill_status_t razobill_template(
    const char* message,
    size_t message_len,
    char* out_buf,
    size_t buf_cap,
    size_t* out_len
);

/**
 * Releases heap memory allocated for a record by razobill_split.
 * Safe to call with NULL or on zeroed records.
 */
RAZOBILL_API void razobill_free(razobill_record_t* record);

/**
 * Returns a static human-readable description for a status code.
 */
RAZOBILL_API const char* razobill_status_message(razobill_status_t status);

/**
 * Returns the runtime library semantic version string (e.g. "2.0.0").
 */
RAZOBILL_API const char* razobill_version(void);

#ifdef __cplusplus
}
#endif

#endif /* RAZOBILL_H */
