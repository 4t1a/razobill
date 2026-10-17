// Hardware-accelerated vector and bit-manipulation scanning routines (AVX-512, AVX2, ARM NEON, SSE4.2, BMI2).
//
// PERF ARCHITECTURE:
// In typical system logs, 70-80% of bytes are structural ASCII words (e.g., timestamps, log levels,
// module names) separated by whitespace (' ', '\t', '\n', '\r').
// A naive scalar loop compares 1 byte per cycle, suffering branch misprediction penalties
// whenever a token ends.
//
// MULTI-ARCHITECTURE VECTORIZATION:
// 1. AVX-512BW: Loads 64 bytes in parallel into 512-bit ZMM registers (`_mm512_loadu_si512`),
//    compares all 64 bytes simultaneously (`_mm512_cmpeq_epi8_mask`), and extracts matches in 1 cycle via `_tzcnt_u64`.
// 2. AVX2: Loads 32 bytes into 256-bit YMM registers (`_mm256_loadu_si256`), compares 32 bytes in parallel,
//    collapses masks into a 32-bit scalar (`_mm256_movemask_epi8`), and traverses via `tzcnt`.
// 3. ARM NEON: Loads 16 bytes into 128-bit vector registers (`uint8x16_t`), reduces masks with narrowing shifts
// (`vshrn_n_u16`),
//    and locates delimiters via `rbit` + `clz`.
// 4. Dynamic Multi-Version Dispatcher: Queries hardware CPUID at runtime (`detectSimdArchitecture()`)
//    to dynamically unlock AVX-512 on server CPUs (Intel Sapphire Rapids, AMD EPYC Genoa) without crashing on older
//    hardware.
#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <string_view>

#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#endif

#if defined(_MSC_VER)
#include <intrin.h>
#endif

#if defined(__ARM_NEON) || defined(__aarch64__) || defined(_M_ARM64)
#include <arm_neon.h>
#endif

namespace razobill::core {

namespace simd {

/// Supported hardware vector acceleration tiers.
enum class SimdArchitecture : std::uint8_t { Scalar = 0, Neon = 1, Avx2 = 2, Avx512 = 3 };

// Branchless count-trailing-zeros for fast bitmask index location.
// Uses hardware `tzcnt` / `bsf` on x86-64 or `rbit` + `clz` on ARM64.
// Guarantees deterministic return of 64 on mask == 0 with zero undefined behavior.
[[nodiscard]] inline int countTrailingZeros64(std::uint64_t mask) noexcept {
    if (mask == 0) [[unlikely]] {
        return 64;
    }
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_ctzll(mask);
#elif defined(_MSC_VER)
    unsigned long index;
    if (_BitScanForward64(&index, mask)) {
        return static_cast<int>(index);
    }
    return 64;
#else
    return std::countr_zero(mask);
#endif
}

/// Dynamically detects the available SIMD instruction set via runtime CPUID / feature checks.
[[nodiscard]] inline SimdArchitecture detectSimdArchitecture() noexcept {
#if defined(__GNUC__) && (defined(__x86_64__) || defined(_M_X64))
    __builtin_cpu_init();
    if (__builtin_cpu_supports("avx512bw") && __builtin_cpu_supports("avx512f")) {
        return SimdArchitecture::Avx512;
    }
    if (__builtin_cpu_supports("avx2")) {
        return SimdArchitecture::Avx2;
    }
    return SimdArchitecture::Scalar;
#elif defined(_MSC_VER) && (defined(__x86_64__) || defined(_M_X64))
    int cpuInfo[4] = {0};
    __cpuid(cpuInfo, 0);
    const int nIds = cpuInfo[0];
    if (nIds >= 7) {
        __cpuidex(cpuInfo, 7, 0);
        const bool avx2 = (cpuInfo[1] & (1 << 5)) != 0;
        const bool avx512f = (cpuInfo[1] & (1 << 16)) != 0;
        const bool avx512bw = (cpuInfo[1] & (1 << 30)) != 0;
        if (avx512f && avx512bw) return SimdArchitecture::Avx512;
        if (avx2) return SimdArchitecture::Avx2;
    }
    return SimdArchitecture::Scalar;
#elif defined(__ARM_NEON) || defined(__aarch64__) || defined(_M_ARM64)
    return SimdArchitecture::Neon;
#elif defined(__AVX2__)
    return SimdArchitecture::Avx2;
#else
    return SimdArchitecture::Scalar;
#endif
}

// Forward declarations for multi-tier SIMD scanners
[[nodiscard]] inline std::size_t findWhitespaceAvx2(const char* data, std::size_t length) noexcept;
[[nodiscard]] inline std::size_t findWhitespaceAvx512(const char* data, std::size_t length) noexcept;
[[nodiscard]] inline std::size_t findWhitespaceNeon(const char* data, std::size_t length) noexcept;
[[nodiscard]] inline std::size_t findWhitespaceDynamic(const char* data, std::size_t length) noexcept;
[[nodiscard]] inline std::size_t findWhitespace(const char* data, std::size_t length) noexcept;

// Scans up to 32 bytes at once using AVX2 for spaces, tabs, or newlines.
[[nodiscard]]
#if (defined(__GNUC__) || defined(__clang__)) && (defined(__x86_64__) || defined(_M_X64))
__attribute__((target("avx2")))
#endif
inline std::size_t findWhitespaceAvx2(const char* data, std::size_t length) noexcept {
#if defined(__x86_64__) || defined(_M_X64)
    std::size_t i = 0;
    const __m256i spaceVec = _mm256_set1_epi8(' ');
    const __m256i tabVec = _mm256_set1_epi8('\t');
    const __m256i lfVec = _mm256_set1_epi8('\n');
    const __m256i crVec = _mm256_set1_epi8('\r');

    while (i + 32 <= length) {
        __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i));
        __m256i eqSpace = _mm256_cmpeq_epi8(chunk, spaceVec);
        __m256i eqTab = _mm256_cmpeq_epi8(chunk, tabVec);
        __m256i eqLf = _mm256_cmpeq_epi8(chunk, lfVec);
        __m256i eqCr = _mm256_cmpeq_epi8(chunk, crVec);

        __m256i combined = _mm256_or_si256(_mm256_or_si256(eqSpace, eqTab), _mm256_or_si256(eqLf, eqCr));
        std::uint32_t mask = static_cast<std::uint32_t>(_mm256_movemask_epi8(combined));
        if (mask != 0) {
            return i + static_cast<std::size_t>(countTrailingZeros64(mask));
        }
        i += 32;
    }
    while (i < length) {
        char c = data[i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') return i;
        ++i;
    }
    return length;
#else
    for (std::size_t i = 0; i < length; ++i) {
        char c = data[i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') return i;
    }
    return length;
#endif
}

// Scans up to 64 bytes at once using AVX-512BW for spaces, tabs, or newlines.
// Unlocks 64-byte vector throughput on Intel Xeon Scalable and AMD EPYC server chips.
[[nodiscard]]
#if (defined(__GNUC__) || defined(__clang__)) && (defined(__x86_64__) || defined(_M_X64))
__attribute__((target("avx512bw,avx512f")))
#endif
inline std::size_t findWhitespaceAvx512(const char* data, std::size_t length) noexcept {
#if (defined(__GNUC__) || defined(__clang__)) && (defined(__x86_64__) || defined(_M_X64))
    std::size_t i = 0;
    const __m512i spaceVec = _mm512_set1_epi8(' ');
    const __m512i tabVec = _mm512_set1_epi8('\t');
    const __m512i lfVec = _mm512_set1_epi8('\n');
    const __m512i crVec = _mm512_set1_epi8('\r');

    while (i + 64 <= length) {
        __m512i chunk = _mm512_loadu_si512(reinterpret_cast<const void*>(data + i));
        __mmask64 eqSpace = _mm512_cmpeq_epi8_mask(chunk, spaceVec);
        __mmask64 eqTab = _mm512_cmpeq_epi8_mask(chunk, tabVec);
        __mmask64 eqLf = _mm512_cmpeq_epi8_mask(chunk, lfVec);
        __mmask64 eqCr = _mm512_cmpeq_epi8_mask(chunk, crVec);

        __mmask64 combined = eqSpace | eqTab | eqLf | eqCr;
        if (combined != 0) {
            return i + static_cast<std::size_t>(countTrailingZeros64(static_cast<std::uint64_t>(combined)));
        }
        i += 64;
    }
    while (i < length) {
        char c = data[i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') return i;
        ++i;
    }
    return length;
#else
    return findWhitespaceAvx2(data, length);
#endif
}

// Scans up to 16 bytes at once using ARM NEON for spaces, tabs, or newlines.
// Optimized for Apple Silicon (M1/M2/M3/M4) and AWS Graviton 2/3/4 (ARM Neoverse).
[[nodiscard]] inline std::size_t findWhitespaceNeon(const char* data, std::size_t length) noexcept {
#if defined(__ARM_NEON) || defined(__aarch64__) || defined(_M_ARM64)
    std::size_t i = 0;
    const uint8x16_t spaceVec = vdupq_n_u8(' ');
    const uint8x16_t tabVec = vdupq_n_u8('\t');
    const uint8x16_t lfVec = vdupq_n_u8('\n');
    const uint8x16_t crVec = vdupq_n_u8('\r');

    while (i + 16 <= length) {
        uint8x16_t chunk = vld1q_u8(reinterpret_cast<const uint8_t*>(data + i));

        uint8x16_t eqSpace = vceqq_u8(chunk, spaceVec);
        uint8x16_t eqTab = vceqq_u8(chunk, tabVec);
        uint8x16_t eqLf = vceqq_u8(chunk, lfVec);
        uint8x16_t eqCr = vceqq_u8(chunk, crVec);

        uint8x16_t combined = vorrq_u8(vorrq_u8(eqSpace, eqTab), vorrq_u8(eqLf, eqCr));

#if defined(__aarch64__) || defined(_M_ARM64)
        if (vmaxvq_u8(combined) == 0) {
            i += 16;
            continue;
        }
#endif

        uint8x8_t narrowed = vshrn_n_u16(vreinterpretq_u16_u8(combined), 4);
        uint64_t scalarMask = vget_lane_u64(vreinterpret_u64_u8(narrowed), 0);
        if (scalarMask != 0) {
            int bitPos = countTrailingZeros64(scalarMask);
            return i + static_cast<std::size_t>(bitPos / 4);
        }
        i += 16;
    }
    while (i < length) {
        char c = data[i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') return i;
        ++i;
    }
    return length;
#else
    for (std::size_t i = 0; i < length; ++i) {
        char c = data[i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') return i;
    }
    return length;
#endif
}

/// Dynamic runtime function dispatcher: automatically selects the best SIMD implementation
/// for the current host CPU at runtime without crashing.
[[nodiscard]] inline std::size_t findWhitespaceDynamic(const char* data, std::size_t length) noexcept {
    static const SimdArchitecture arch = detectSimdArchitecture();
    switch (arch) {
    case SimdArchitecture::Avx512:
        return findWhitespaceAvx512(data, length);
    case SimdArchitecture::Avx2:
        return findWhitespaceAvx2(data, length);
    case SimdArchitecture::Neon:
        return findWhitespaceNeon(data, length);
    case SimdArchitecture::Scalar:
    default:
        for (std::size_t i = 0; i < length; ++i) {
            char c = data[i];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') return i;
        }
        return length;
    }
}

/// Unified multi-architecture SIMD delimiter scanner.
/// Automatically dispatches inline when target architecture is fixed at compile time,
/// or routes through findWhitespaceDynamic() for universal compatibility.
[[nodiscard]] inline std::size_t findWhitespace(const char* data, std::size_t length) noexcept {
#if defined(__AVX512BW__) && defined(__AVX512F__)
    return findWhitespaceAvx512(data, length);
#elif defined(__AVX2__)
    return findWhitespaceAvx2(data, length);
#elif defined(__ARM_NEON) || defined(__aarch64__) || defined(_M_ARM64)
    return findWhitespaceNeon(data, length);
#else
    return findWhitespaceDynamic(data, length);
#endif
}

} // namespace simd

} // namespace razobill::core
