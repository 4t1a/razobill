// Status codes of the data plane.
// Pure, deterministic, zero-allocation error reporting.
//
// ARCHITECTURE: Why No C++ Exceptions on the Data Plane?
// Throwing a C++ exception takes ~2,000 to 7,000 nanoseconds because the CPU must unwind
// the stack, read DWARF exception tables (.eh_frame), and allocate heap memory.
// In a high-throughput stream processing 70M msgs/sec, that is catastrophic.
//
// By using `enum class Status : unsigned char`, every return code fits inside an 8-bit
// CPU register (`%al`). Branches predict perfectly and errors cost zero heap allocations.
// Deterministic error handling:
// When malformed logs occur in high-throughput streams, returning a compact 1-byte
// enum maintains steady-state cache performance without invoking stack unwinders.
#pragma once

namespace razobill {

enum class Status : unsigned char {
    Ok = 0,
    TooLarge,       // message or rebuilt message exceeds kMaxMessageBytes (16 MB)
    Malformed,      // form key is not "<skeleton>\x13<types>" or counts disagree
    SlotUnderflow,  // fewer integer or string slots supplied than the form requires
    SlotOverflow,   // more integer or string slots supplied than the form requires
    BufferTooSmall, // destination buffer capacity is insufficient
    InvalidParam    // null pointer or out-of-range argument
};

[[nodiscard]] constexpr const char* describe(Status s) noexcept {
    switch (s) {
    case Status::Ok:
        return "ok";
    case Status::TooLarge:
        return "message exceeds the size limit";
    case Status::Malformed:
        return "malformed form key";
    case Status::SlotUnderflow:
        return "fewer slot values than the form requires";
    case Status::SlotOverflow:
        return "more slot values than the form requires";
    case Status::BufferTooSmall:
        return "destination buffer capacity is insufficient";
    case Status::InvalidParam:
        return "invalid parameter or null pointer";
    }
    return "unknown";
}

} // namespace razobill
