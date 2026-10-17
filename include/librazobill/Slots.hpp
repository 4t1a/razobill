// Data Plane: message <-> (template, integer slots, string slots). Pure, reentrant, exact.
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
// PERF & LIFETIME CONTRACT (F-MEM-01):
// `strs` contains zero-copy `std::string_view` referencing the original message slice.
// The caller's source message buffer MUST outlive `SlotRecord` and remain unmodified
// while `strs` elements are accessed. Callers requiring detached lifetimes must explicitly
// copy the string slices into owning containers.
struct SlotRecord {
    std::string templateStr;            // human-readable format string with {i} and {s} placeholders
    std::vector<std::int64_t> ints;     // values of the '{i}' slots
    std::vector<std::string_view> strs; // values of the '{s}' slots; zero-copy views into caller message buffer

    // Legacy fields for internal backward compatibility
    std::string skeleton;
    std::string types;

    // Bulk clear for scratch recycling
    void clear() noexcept {
        templateStr.clear();
        ints.clear();
        strs.clear();
        skeleton.clear();
        types.clear();
    }

    void formKey(std::string& out) const { out.assign(templateStr); }

    [[nodiscard]] std::string formKey() const {
        std::string res;
        formKey(res);
        return res;
    }

    [[nodiscard]] std::uint64_t formKeyHash() const noexcept { return xxh64(formKey()); }

    void form_key(std::string& out) const { formKey(out); }
};

using Slots = SlotRecord;

// Splits msg into template and slots. out.strs borrows from msg.
[[nodiscard]] Status parse(std::string_view msg, SlotRecord& out, ScanMode mode = ScanMode::Semantic);

// Reconstructs the original message from a template and slots with 100% byte-for-byte fidelity.
[[nodiscard]] Status reconstruct(std::string_view tmpl, std::span<const std::int64_t> ints,
                                 std::span<const std::string_view> strs, std::string& out);

// Alias for reconstruct
[[nodiscard]] inline Status join(std::string_view tmpl, std::span<const std::int64_t> ints,
                                 std::span<const std::string_view> strs, std::string& out) {
    return reconstruct(tmpl, ints, strs, out);
}

// Generates template representation directly from input message or form
[[nodiscard]] Status templateOf(std::string_view form, std::string& out);

// Alias for templateOf
[[nodiscard]] inline Status template_of(std::string_view form, std::string& out) {
    return templateOf(form, out);
}

} // namespace razobill::core
