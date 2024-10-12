#pragma once
// Version and ABI specification for librazobill.
// Follows strict Semantic Versioning 2.0.0 (MAJOR.MINOR.PATCH).
//
// GOVERNANCE & THE ABI CONTRACT:
// Software versioning is an explicit contract between maintainers and consumers.
// 1. API Compatibility: Source-level. A minor version bump guarantees consumer C++ code compiles.
// 2. ABI Compatibility: Binary/symbol-level. Crucial for shared libraries (.dll, .so).
//    As long as LIBRAZOBILL_ABI_VERSION remains unchanged, consumers can swap shared binaries
//    without recompiling their host applications.
//
// Breaking changes are forbidden without incrementing MAJOR.
#include <cstdint>
#include <string_view>

#define LIBRAZOBILL_VERSION_MAJOR 1
#define LIBRAZOBILL_VERSION_MINOR 0
#define LIBRAZOBILL_VERSION_PATCH 0
#define LIBRAZOBILL_VERSION_STRING "1.0.0"
#define LIBRAZOBILL_ABI_VERSION 1

#if defined(__cplusplus)
#define LIBRAZOBILL_DEPRECATED(msg) [[deprecated(msg)]]
#elif defined(_MSC_VER)
#define LIBRAZOBILL_DEPRECATED(msg) __declspec(deprecated(msg))
#elif defined(__GNUC__) || defined(__clang__)
#define LIBRAZOBILL_DEPRECATED(msg) __attribute__((deprecated(msg)))
#else
#define LIBRAZOBILL_DEPRECATED(msg)
#endif

namespace razobill {

struct VersionInfo {
    std::uint32_t major{LIBRAZOBILL_VERSION_MAJOR};
    std::uint32_t minor{LIBRAZOBILL_VERSION_MINOR};
    std::uint32_t patch{LIBRAZOBILL_VERSION_PATCH};
    std::uint32_t abi{LIBRAZOBILL_ABI_VERSION};
    const char* string{LIBRAZOBILL_VERSION_STRING};
};

[[nodiscard]] constexpr VersionInfo version() noexcept {
    return VersionInfo{};
}

[[nodiscard]] constexpr std::uint32_t abiVersion() noexcept {
    return LIBRAZOBILL_ABI_VERSION;
}

[[nodiscard]] constexpr std::string_view versionString() noexcept {
    return LIBRAZOBILL_VERSION_STRING;
}

} // namespace razobill
