// Data Plane: message <-> (form, integer slots, string slots). Pure, reentrant, exact.
//
// ARCHITECTURE: Structure-of-Arrays (SoA) Log Decomposition
// Most log parsers treat a parsed line as an object with dynamic attribute dictionaries
// (e.g. `{"ip": "1.2.3.4", "port": 8080}`). That allocates dozens of tiny heap objects per log!
//
// librazobill decomposes every log into two orthogonal planes:
// 1. Structural Form: An immutable skeleton string with ASCII placeholders + a type signature string.
// 2. Variable Slots: Contiguous vectors of primitive numbers (`ints`) and zero-copy string views (`strs`).
//
// This maps directly to columnar database engines (DuckDB / ClickHouse / Parquet):
// - `ints` compress with frame-of-reference (FOR) bitpacking.
// - `strs` compress with dictionary or Snappy block encoding.
// - `skeleton` compresses to near zero because duplicate logs share identical forms.
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "librazobill/Hash.hpp"
#include "librazobill/Scanner.hpp"
#include "librazobill/Status.hpp"

namespace razobill::core {

// Structure-of-arrays representation of parsed log slots.
//
// PERF:
// `strs` contains zero-copy `std::string_view` referencing the original message slice.
// As long as the source message string is alive during batch processing, no string
// copies occur during parsing.
struct SlotRecord {
    std::string skeleton;               // constant text, one kPlaceholder per slot
    std::string types;                  // one 'I' or 'S' per slot, in order
    std::vector<std::int64_t> ints;     // values of the 'I' slots
    std::vector<std::string_view> strs; // values of the 'S' slots; zero-copy views into the message

    // Bulk clear for thread-local scratch recycling.
    // Capacity is preserved so subsequent lines don't trigger re-allocations!
    void clear() noexcept {
        skeleton.clear();
        types.clear();
        ints.clear();
        strs.clear();
    }

    // Combines skeleton and type signatures into a canonical form key.
    // Format: "<skeleton>\x13<types>"
    void formKey(std::string& out) const {
        out.assign(skeleton);
        out.push_back(static_cast<char>(kTypeSeparator));
        out.append(types);
    }

    [[nodiscard]] std::string formKey() const {
        std::string res;
        formKey(res);
        return res;
    }

    [[nodiscard]] std::uint64_t formKeyHash() const noexcept {
        std::string key = formKey();
        return xxh64(key);
    }

    // Legacy compatibility methods
    void form_key(std::string& out) const { formKey(out); }
};

using Slots = SlotRecord;

// Splits msg into form and slots. out.strs borrows from msg.
[[nodiscard]] Status parse(std::string_view msg, SlotRecord& out, ScanMode mode = ScanMode::Semantic);

// Reconstructs the original message from a form key and slots with 100% byte-for-byte fidelity.
[[nodiscard]] Status reconstruct(std::string_view form, std::span<const std::int64_t> ints,
                                 std::span<const std::string_view> strs, std::string& out);

// Legacy alias for reconstruct
[[nodiscard]] inline Status join(std::string_view form, std::span<const std::int64_t> ints,
                                 std::span<const std::string_view> strs, std::string& out) {
    return reconstruct(form, ints, strs, out);
}

// Generates human-readable template representation with "<*>" placeholders.
[[nodiscard]] Status templateOf(std::string_view form, std::string& out);

// Legacy alias for templateOf
[[nodiscard]] inline Status template_of(std::string_view form, std::string& out) {
    return templateOf(form, out);
}

} // namespace razobill::core
