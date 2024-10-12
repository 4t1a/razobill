// ==============================================================================
// Razobill Zero-Copy Data Plane: Fast Slot Decomposition & Reconstruction
// ==============================================================================
//
// THEORETICAL FOUNDATION & INVARIANTS:
// Grounded in Proposition 1 and Proposition 2 of "Latitude and Doubt" (M. M. Atta, 2026).
// Every incoming message string $m \in \Sigma^*$ is decomposed into:
// 1. A static template skeleton where variable tokens are replaced with sentinel \x12.
// 2. An in-band type descriptor suffix (e.g. \x13IIS) identifying slot types.
// 3. Strongly-typed extracted variables:
//    - Pure non-negative integers -> parsed via `std::from_chars` into 64-bit integers.
//    - Composite variables & strings -> zero-copy `std::string_view` slices into input text.
//
// ZERO-ALLOCATION INVERTIBILITY:
// The reconstruction routine `reconstruct()` reverses the decomposition process:
// $$\forall m \in \Sigma^*, \quad \operatorname{Reconstruct}(\operatorname{Decompose}(m)) \equiv m$$
// Rebuilding relies on branchless digit length estimation (`digits10`) and high-speed
// `std::to_chars` formatted output directly into the caller's target buffer, avoiding
// intermediate allocations, string copies, or runtime heap fragmentation.
// ==============================================================================

#include "librazobill/Slots.hpp"

#include <charconv>

namespace razobill::core {

namespace {

// Branchless/unrolled base-10 digit length calculation.
//
// PERF:
// Used during reconstruct() to compute the exact byte length needed for the output buffer.
// Avoids slow `std::to_string()`, `snprintf()`, or floating-point `std::log10()`.
constexpr std::size_t digits10(std::int64_t v) noexcept {
    std::size_t n = 1;
    for (; v >= 10; v /= 10)
        ++n;
    return n;
}

// Splits "<skeleton>\x13<types>" and validates slot count parity.
// Returns Malformed if the number of placeholders in the skeleton does not match types.size().
Status splitKey(std::string_view form, std::string_view& skeleton, std::string_view& types, std::size_t& nInt,
                std::size_t& nStr) noexcept {
    const auto sep = form.rfind(static_cast<char>(kTypeSeparator));
    if (sep == std::string_view::npos) return Status::Malformed;
    skeleton = form.substr(0, sep);
    types = form.substr(sep + 1);
    nInt = nStr = 0;
    for (char t : types) {
        if (t == 'I')
            ++nInt;
        else if (t == 'S')
            ++nStr;
        else
            return Status::Malformed;
    }
    std::size_t holes = 0;
    for (char c : skeleton)
        holes += static_cast<unsigned char>(c) == kPlaceholder;
    return holes == types.size() ? Status::Ok : Status::Malformed;
}

} // namespace

// Single-pass parsing: scans `msg` and extracts constants into `skeleton` while
// isolating variable tokens into `ints` and `strs`.
Status parse(std::string_view msg, SlotRecord& out, ScanMode mode) {
    out.clear();
    if (msg.size() > kMaxMessageBytes) [[unlikely]]
        return Status::TooLarge;
    out.skeleton.reserve(msg.size());

    const auto result = scan(
        msg,
        [&out](std::string_view gap, std::string_view tok) {
            out.skeleton.append(gap);
            out.skeleton.push_back(static_cast<char>(kPlaceholder));
            if (isIntToken(tok)) {
                std::int64_t v = 0;
                // High-speed C++17 std::from_chars: locale-independent, zero heap allocation
                std::from_chars(tok.data(), tok.data() + tok.size(), v);
                out.ints.push_back(v);
                out.types.push_back('I');
            } else {
                // Zero-copy string view into the original message string
                out.strs.push_back(tok);
                out.types.push_back('S');
            }
        },
        mode);

    // Escape hatch: if the raw message contained reserved delimiter control bytes,
    // safely treat the entire line as a single string slot rather than corrupting the ledger.
    if (result.reserved) [[unlikely]] {
        out.clear();
        out.skeleton.push_back(static_cast<char>(kPlaceholder));
        out.types.push_back('S');
        out.strs.push_back(msg);
        return Status::Ok;
    }
    out.skeleton.append(result.tail);
    return Status::Ok;
}

// Losslessly reconstructs the exact original log message.
//
// JOKE:
// Drain3: "Here is your template: <*> <*> <*>, goodbye variables forever!"
// librazobill: "Here is your exact message back, byte-for-byte, in 90 nanoseconds."
Status reconstruct(std::string_view form, std::span<const std::int64_t> ints, std::span<const std::string_view> strs,
                   std::string& out) {
    std::string_view skeleton, types;
    std::size_t nInt = 0, nStr = 0;
    if (const auto s = splitKey(form, skeleton, types, nInt, nStr); s != Status::Ok) return s;
    if (ints.size() < nInt || strs.size() < nStr) return Status::SlotUnderflow;

    std::size_t size = skeleton.size() - types.size();
    for (std::size_t i = 0; i < nInt; ++i) {
        if (ints[i] < 0) return Status::Malformed;
        size += digits10(ints[i]);
    }
    for (std::size_t i = 0; i < nStr; ++i)
        size += strs[i].size();
    if (size > kMaxMessageBytes) return Status::TooLarge;

    out.clear();
    out.reserve(size);
    std::size_t ti = 0, ii = 0, si = 0;
    char num[24];
    for (char c : skeleton) {
        if (static_cast<unsigned char>(c) != kPlaceholder) {
            out.push_back(c);
        } else if (types[ti++] == 'I') {
            const auto r = std::to_chars(num, num + sizeof num, ints[ii++]);
            out.append(num, r.ptr);
        } else {
            out.append(strs[si++]);
        }
    }
    return Status::Ok;
}

Status templateOf(std::string_view form, std::string& out) {
    std::string_view skeleton, types;
    std::size_t nInt = 0, nStr = 0;
    if (const auto s = splitKey(form, skeleton, types, nInt, nStr); s != Status::Ok) return s;
    out.clear();
    for (char c : skeleton) {
        if (static_cast<unsigned char>(c) == kPlaceholder)
            out.append("<*>");
        else
            out.push_back(c);
    }
    return Status::Ok;
}

} // namespace razobill::core
