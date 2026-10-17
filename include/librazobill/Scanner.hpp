// High-performance normative tokenizer and lexical finite-state machine.
// Scans byte-wise without dynamic memory allocation or backtracking.
//
// ARCHITECTURE: Why No Regular Expressions (Regex)?
// Some developers say: "I need to parse logs, I will write a regex."
// Now they have two problems, 50k msgs/sec throughput, and an overheating CPU core.
// Standard PCRE / std::regex engines allocate graph state nodes and suffer from backtracking.
//
// Here, we use a compile-time constexpr 256-byte classification table (`detail::kClass`).
// Every character's classification (delimiter, digit, alpha, control) is resolved
// by a single L1-cached byte lookup: `kClass[*p]`.
// The scanner runs in strictly linear O(N) time with zero dynamic heap allocation.
#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "librazobill/SimdScanner.hpp"

namespace razobill::core {

// ASCII control bytes used for internal form key formatting.
//
// DECISION: Why not '{}' or '%s' or '<*>'?
// Because raw server logs frequently contain literal '{}', '%s', or '<*>' (e.g. JSON strings, C format strings).
// Using invisible ASCII control codes:
//   0x12 (Device Control 2) = Slot Placeholder
//   0x13 (Device Control 3) = Type Separator
// guarantees zero collision with printable text. If an adversarial log line happens
// to contain 0x11..0x13, our escape hatch safely wraps it without crashing.
inline constexpr unsigned char kPlaceholder = 0x12;   // marks a variable slot inside a skeleton
inline constexpr unsigned char kTypeSeparator = 0x13; // separates skeleton from type string in a form key
inline constexpr std::size_t kMaxMessageBytes = std::size_t{16} << 20;

enum class ScanMode : std::uint8_t {
    Exact = 0,     // Pure digit-bearing variable tokens
    DigitOnly = 0, // Alias for Exact
    Semantic = 1   // Multi-modal: digits, key-value assignments, network hosts, paths, contextual keywords
};

namespace detail {

inline constexpr unsigned char kDelim = 1, kDigit = 2, kReserved = 4, kAlpha = 8;

// Compile-time precomputed 256-element ASCII classifier array.
// Evaluated during compilation by constexpr; resides in read-only .rodata.
constexpr std::array<unsigned char, 256> makeClasses() {
    std::array<unsigned char, 256> t{};
    for (char c :
         {'\t', '\n', '\f', '\r', ' ', '=', ',', ';', ':', '(', ')', '[', ']', '{', '}', '"', '\'', '<', '>', '|', '/'})
        t[static_cast<unsigned char>(c)] = kDelim;
    for (unsigned c = '0'; c <= '9'; ++c)
        t[c] = kDigit;
    for (unsigned c = 'a'; c <= 'z'; ++c)
        t[c] = kAlpha;
    for (unsigned c = 'A'; c <= 'Z'; ++c)
        t[c] = kAlpha;
    t['_'] = kAlpha;
    // Bytes 0x11..0x13 are reserved for internal form key framing. Messages containing one are escaped.
    for (unsigned c = 0x11; c <= 0x13; ++c)
        t[c] = kReserved;
    return t;
}

inline constexpr std::array<unsigned char, 256> kClass = makeClasses();

// Context keyword detector (e.g., "user", "from", "for", "host").
// If a word like "user" precedes a non-numeric token ("alice"), Semantic mode
// knows "alice" is a variable parameter rather than static template syntax!
inline constexpr std::size_t kMaxKeywordLen = 10;

inline bool isContextKeyword(std::string_view w) noexcept {
    if (w.size() < 2 || w.size() > kMaxKeywordLen) return false;
    std::array<char, 12> buf{};
    static_assert(kMaxKeywordLen < buf.size(), "kMaxKeywordLen must fit inside stack buffer");
    for (std::size_t i = 0; i < w.size(); ++i) {
        char c = w[i];
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
        buf[i] = c;
    }
    std::string_view s(buf.data(), w.size());
    return s == "for" || s == "user" || s == "by" || s == "from" || s == "ruser" || s == "rhost" || s == "host" ||
           s == "account" || s == "client" || s == "src" || s == "dest" || s == "to" || s == "at" || s == "proxy" ||
           s == "server" || s == "node" || s == "target" || s == "app" || s == "device";
}

} // namespace detail

struct ScanResult {
    std::string_view tail; // constant text trailing after the final token
    bool reserved;         // true if message contains reserved byte (0x11, 0x12, 0x13)
};

// Zero-allocation single-pass scanner
template <class OnToken>
[[nodiscard]] inline ScanResult scan(std::string_view m, OnToken&& onToken,
                                     ScanMode mode = ScanMode::Semantic) noexcept(noexcept(onToken(m, m))) {
    const auto* p = reinterpret_cast<const unsigned char*>(m.data());
    const auto* const end = p + m.size();
    const auto* gap = p;
    unsigned char all = 0;
    std::string_view lastWord;

    const unsigned char* unclosedDoubleQuoteEnd = nullptr;
    const unsigned char* unclosedSingleQuoteEnd = nullptr;

    while (p < end) {
        const auto* const dStart = p;
        while (p < end && (detail::kClass[*p] & detail::kDelim)) {
            if (mode == ScanMode::Semantic && (*p == '"' || *p == '\'')) {
                const unsigned char quote = *p;
                if ((quote == '"' && p < unclosedDoubleQuoteEnd) || (quote == '\'' && p < unclosedSingleQuoteEnd)) {
                    // Suffix already confirmed to have no matching unescaped closing quote; treat as regular delimiter
                    all |= detail::kClass[*p];
                    ++p;
                    continue;
                }
                break; // Potential quoted string literal
            }
            all |= detail::kClass[*p];
            ++p;
        }
        const auto* const dEnd = p;
        const auto dLen = static_cast<std::size_t>(dEnd - dStart);
        const char lastDelimChar = dLen > 0 ? static_cast<char>(*(dEnd - 1)) : '\0';

        if (p >= end) break;

        // Quoted string literal handling in Semantic mode
        if (mode == ScanMode::Semantic && (*p == '"' || *p == '\'')) {
            const unsigned char quote = *p;
            const auto* const qStart = p;
            ++p;
            while (p < end && *p != quote && *p != '\n') {
                if (*p == '\\' && p + 1 < end)
                    p += 2;
                else
                    ++p;
            }
            if (p < end && *p == quote) {
                ++p; // include closing quote
                for (const auto* cp = qStart; cp < p; ++cp)
                    all |= detail::kClass[*cp];
                const std::string_view tok(reinterpret_cast<const char*>(qStart), static_cast<std::size_t>(p - qStart));
                onToken(std::string_view(reinterpret_cast<const char*>(gap), static_cast<std::size_t>(qStart - gap)),
                        tok);
                gap = p;
                lastWord = {};
                continue;
            } else {
                if (quote == '"')
                    unclosedDoubleQuoteEnd = p;
                else
                    unclosedSingleQuoteEnd = p;
                all |= detail::kClass[quote];
                p = qStart + 1; // Unclosed quote: proceed past qStart
                continue;
            }
        }

        const auto* const start = p;
        unsigned char seen = 0;
        while (p < end && !(detail::kClass[*p] & detail::kDelim))
            seen |= detail::kClass[*p++];
        all |= seen;

        const std::string_view tok(reinterpret_cast<const char*>(start), static_cast<std::size_t>(p - start));
        bool isVar = false;

        if (seen & detail::kDigit) {
            isVar = true;
        } else if (mode == ScanMode::Semantic) {
            if (lastDelimChar == '=' || lastDelimChar == '@') {
                isVar = true;
            } else if (lastDelimChar == '"' || lastDelimChar == '\'') {
                isVar = true;
            } else if (!lastWord.empty() && detail::isContextKeyword(lastWord)) {
                isVar = true;
            } else if (p < end && *p == ':' && tok.find('.') != std::string_view::npos) {
                isVar = true;
            } else if (tok.size() > 4 && (tok.ends_with(".exe") || tok.ends_with(".dll") || tok.ends_with(".so"))) {
                isVar = true;
            }
        }

        if (isVar) {
            onToken(std::string_view(reinterpret_cast<const char*>(gap), static_cast<std::size_t>(start - gap)), tok);
            gap = p;
            lastWord = {};
        } else {
            lastWord = tok;
        }
    }
    return {std::string_view(reinterpret_cast<const char*>(gap), static_cast<std::size_t>(end - gap)),
            (all & detail::kReserved) != 0};
}

// Decimal integer validator
// DESIGN DECISION (F-SLOT-01):
// Tokens with length <= 18 decimal digits (max 999,999,999,999,999,999) are guaranteed to strictly fit
// within signed int64_t without overflow (INT64_MAX is 9,223,372,036,854,775,807, which is 19 digits).
// Bounding at 18 digits avoids complex lexicographical comparisons against INT64_MAX on the critical
// tokenizer hot path, safely categorizing larger numeric sequences into string slots without truncation.
[[nodiscard]] constexpr bool isIntToken(std::string_view t) noexcept {
    if (t.empty() || t.size() > 18) return false;
    if (t.front() == '0') return t.size() == 1;
    return std::ranges::all_of(t, [](char c) { return c >= '0' && c <= '9'; });
}

} // namespace razobill::core
