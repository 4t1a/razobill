// ==============================================================================
// Razobill Zero-Copy Data Plane: Fast Slot Decomposition & Reconstruction
// ==============================================================================

#include "librazobill/Slots.hpp"

#include <charconv>

namespace razobill::core {

namespace {

constexpr std::size_t digits10(std::int64_t v) noexcept {
    std::size_t n = (v < 0) ? 2 : 1;
    uint64_t u = (v < 0) ? static_cast<uint64_t>(-(v + 1)) + 1 : static_cast<uint64_t>(v);
    for (; u >= 10; u /= 10)
        ++n;
    return n;
}

bool isLegacyFormKey(std::string_view form) noexcept {
    if (form.find("{i}") != std::string_view::npos || form.find("{s}") != std::string_view::npos ||
        form.find("{{") != std::string_view::npos || form.find("}}") != std::string_view::npos) {
        return false;
    }
    const auto sep = form.rfind(static_cast<char>(kTypeSeparator));
    if (sep == std::string_view::npos || sep == form.size() - 1) return false;
    std::string_view types = form.substr(sep + 1);
    if (types.empty()) return false;
    for (char c : types) {
        if (c != 'I' && c != 'S') return false;
    }
    std::string_view skeleton = form.substr(0, sep);
    std::size_t placeholders = 0;
    for (char c : skeleton) {
        if (static_cast<unsigned char>(c) == kPlaceholder) {
            ++placeholders;
        }
    }
    return placeholders > 0 && placeholders == types.size();
}

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

Status reconstructLegacy(std::string_view form, std::span<const std::int64_t> ints,
                         std::span<const std::string_view> strs, std::string& out) {
    std::string_view skeleton, types;
    std::size_t nInt = 0, nStr = 0;
    if (const auto s = splitKey(form, skeleton, types, nInt, nStr); s != Status::Ok) {
        return s;
    }
    if (ints.size() < nInt || strs.size() < nStr) {
        return Status::SlotUnderflow;
    }
    if (ints.size() > nInt || strs.size() > nStr) {
        return Status::SlotOverflow;
    }

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
    std::array<char, 32> num{};
    for (char c : skeleton) {
        if (static_cast<unsigned char>(c) != kPlaceholder) {
            out.push_back(c);
        } else if (types[ti++] == 'I') {
            const auto r = std::to_chars(num.data(), num.data() + num.size(), ints[ii++]);
            out.append(num.data(), r.ptr);
        } else {
            out.append(strs[si++]);
        }
    }
    return Status::Ok;
}

Status reconstructNew(std::string_view tmpl, std::span<const std::int64_t> ints, std::span<const std::string_view> strs,
                      std::string& out) {
    // DESIGN DECISION (F-SLOT-02):
    // Two-pass reconstruction: Pass 1 calculates the exact output byte size (`estSize`),
    // checks slot underflow/overflow invariants, and prevents buffer reallocation.
    // Pass 2 formats integers and copies string slices directly into the pre-reserved buffer.
    // This eliminates intermediate reallocations and guarantees O(N) allocation determinism.
    const char* p = tmpl.data();
    const char* const end = p + tmpl.size();
    std::size_t nInt = 0, nStr = 0;
    std::size_t estSize = 0;

    while (p < end) {
        if (*p == '{') {
            if (p + 1 < end && *(p + 1) == '{') {
                estSize += 1;
                p += 2;
            } else if (p + 2 < end && *(p + 1) == 'i' && *(p + 2) == '}') {
                if (nInt >= ints.size()) return Status::SlotUnderflow;
                if (ints[nInt] < 0) return Status::Malformed;
                estSize += digits10(ints[nInt++]);
                p += 3;
            } else if (p + 2 < end && *(p + 1) == 's' && *(p + 2) == '}') {
                if (nStr >= strs.size()) return Status::SlotUnderflow;
                estSize += strs[nStr++].size();
                p += 3;
            } else {
                return Status::Malformed;
            }
        } else if (*p == '}') {
            if (p + 1 < end && *(p + 1) == '}') {
                estSize += 1;
                p += 2;
            } else {
                return Status::Malformed;
            }
        } else {
            estSize += 1;
            p += 1;
        }
    }

    if (nInt == 0 && nStr == 0 && (!ints.empty() || !strs.empty())) {
        return Status::Malformed;
    }
    if (ints.size() < nInt || strs.size() < nStr) {
        return Status::SlotUnderflow;
    }
    if (ints.size() > nInt || strs.size() > nStr) {
        return Status::SlotOverflow;
    }
    if (estSize > kMaxMessageBytes) return Status::TooLarge;

    out.clear();
    out.reserve(estSize);

    p = tmpl.data();
    nInt = 0;
    nStr = 0;
    std::array<char, 32> numBuf{};
    while (p < end) {
        if (*p == '{') {
            if (p + 1 < end && *(p + 1) == '{') {
                out.push_back('{');
                p += 2;
            } else if (p + 2 < end && *(p + 1) == 'i' && *(p + 2) == '}') {
                const auto r = std::to_chars(numBuf.data(), numBuf.data() + numBuf.size(), ints[nInt++]);
                out.append(numBuf.data(), r.ptr);
                p += 3;
            } else if (p + 2 < end && *(p + 1) == 's' && *(p + 2) == '}') {
                out.append(strs[nStr++]);
                p += 3;
            } else {
                out.push_back(*p++);
            }
        } else if (*p == '}') {
            if (p + 1 < end && *(p + 1) == '}') {
                out.push_back('}');
                p += 2;
            } else {
                out.push_back(*p++);
            }
        } else {
            out.push_back(*p++);
        }
    }
    return Status::Ok;
}

} // namespace

Status parse(std::string_view msg, SlotRecord& out, ScanMode mode) {
    out.clear();
    if (msg.size() > kMaxMessageBytes) [[unlikely]]
        return Status::TooLarge;
    out.templateStr.reserve(msg.size() + 16);
    out.skeleton.reserve(msg.size() + 16);

    const auto result = scan(
        msg,
        [&out](std::string_view gap, std::string_view tok) {
            out.skeleton.append(gap);
            out.skeleton.push_back(static_cast<char>(kPlaceholder));
            for (char c : gap) {
                if (c == '{')
                    out.templateStr.append("{{");
                else if (c == '}')
                    out.templateStr.append("}}");
                else
                    out.templateStr.push_back(c);
            }
            if (isIntToken(tok)) {
                std::int64_t v = 0;
                std::from_chars(tok.data(), tok.data() + tok.size(), v);
                out.ints.push_back(v);
                out.types.push_back('I');
                out.templateStr.append("{i}");
            } else {
                out.strs.push_back(tok);
                out.types.push_back('S');
                out.templateStr.append("{s}");
            }
        },
        mode);

    out.skeleton.append(result.tail);
    for (char c : result.tail) {
        if (c == '{')
            out.templateStr.append("{{");
        else if (c == '}')
            out.templateStr.append("}}");
        else
            out.templateStr.push_back(c);
    }

    if (result.reserved) {
        // When message contains internal framing control characters (0x11..0x13),
        // legacy skeleton encoding is ambiguous under the legacy wire format.
        // Invalidate legacy skeleton/types to mandate modern templateStr format.
        out.skeleton.clear();
        out.types.clear();
    }

    return Status::Ok;
}

Status reconstruct(std::string_view tmpl, std::span<const std::int64_t> ints, std::span<const std::string_view> strs,
                   std::string& out) {
    if (isLegacyFormKey(tmpl)) {
        return reconstructLegacy(tmpl, ints, strs, out);
    }
    return reconstructNew(tmpl, ints, strs, out);
}

Status templateOf(std::string_view form, std::string& out) {
    if (isLegacyFormKey(form)) {
        std::string_view skeleton, types;
        std::size_t nInt = 0, nStr = 0;
        if (const auto s = splitKey(form, skeleton, types, nInt, nStr); s != Status::Ok) {
            return s;
        }
        out.clear();
        out.reserve(skeleton.size());
        for (char c : skeleton) {
            if (static_cast<unsigned char>(c) == kPlaceholder)
                out.append("<*>");
            else
                out.push_back(c);
        }
        return Status::Ok;
    }

    if (form.find(static_cast<char>(kTypeSeparator)) != std::string_view::npos ||
        form.find(static_cast<char>(kPlaceholder)) != std::string_view::npos) {
        return Status::Malformed;
    }

    out.clear();
    out.reserve(form.size());

    const char* p = form.data();
    const char* const end = p + form.size();

    while (p < end) {
        if (*p == '{') {
            if (p + 1 < end && *(p + 1) == '{') {
                out.push_back('{');
                p += 2;
            } else if (p + 2 < end && (*(p + 1) == 'i' || *(p + 1) == 's') && *(p + 2) == '}') {
                out.append("<*>");
                p += 3;
            } else {
                out.clear();
                return Status::Malformed;
            }
        } else if (*p == '}') {
            if (p + 1 < end && *(p + 1) == '}') {
                out.push_back('}');
                p += 2;
            } else {
                out.clear();
                return Status::Malformed;
            }
        } else {
            out.push_back(*p++);
        }
    }
    return Status::Ok;
}

} // namespace razobill::core
