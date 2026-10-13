// ==============================================================================
// Razobill Data-Plane Micro-Benchmark: Single-Core Throughput & Nanosecond Audit
// ==============================================================================
//
// WHY THIS BENCHMARK EXISTS:
// Measures the raw, unfiltered scalar throughput of the C++20 data plane,
// the Universal C ABI (RzDecompose, RzReconstructSlots), and hardware SIMD scanners
// on a single CPU core without locks or thread-pool queues.
//
// WHAT WE MEASURE:
// 1. C++20 parse (form + slots)
// 2. C++20 parse + formKey serialization
// 3. C++20 parse + formKey + XXH64 hash
// 4. C++20 parse + round-trip reconstruct (lossless verify)
// 5. Universal C ABI RzParse
// 6. Universal C ABI RzDecompose (zero-copy tuple extraction)
// 7. Universal C ABI RzReconstruct
// 8. Universal C ABI RzReconstructSlots (direct slot arrays)
// 9. Hardware SIMD findWhitespace (AVX2 / ARM NEON multi-dispatcher)
// 10. Scalar delimiter search baseline
// 11. XXH64 raw message hash baseline
// ==============================================================================

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include "librazobill/Hash.hpp"
#include "librazobill/RazobillC.h"
#include "librazobill/SimdScanner.hpp"
#include "librazobill/Slots.hpp"

int main(int argc, char** argv) {
    std::string path;
    if (argc > 1 && argv[1] != nullptr) {
        std::error_code ec;
        std::filesystem::path inputPath(argv[1]);
        if (std::filesystem::is_regular_file(inputPath, ec) && !ec) {
            auto canonicalPath = std::filesystem::canonical(inputPath, ec);
            if (!ec) {
                path = canonicalPath.string();
            } else {
                std::printf("[BENCH] Error: Cannot resolve canonical path for '%s'.\n", argv[1]);
                return 1;
            }
        } else {
            std::printf("[BENCH] Error: '%s' is not a valid regular file.\n", argv[1]);
            return 1;
        }
    } else {
        const std::vector<std::string> candidates = {"logs/SSH/SSH.log", "logs/Linux/Linux.log",
                                                     "logs/Zookeeper/Zookeeper.log", "/tmp/corpus.txt"};
        for (const auto& c : candidates) {
            std::ifstream check(c);
            if (check.is_open()) {
                path = c;
                break;
            }
        }
    }

    if (path.empty()) {
        std::puts("[BENCH] Error: No valid corpus log file found.");
        return 1;
    }

    std::ifstream in(path);
    if (!in.is_open()) {
        std::printf("[BENCH] Error: Could not open '%s'.\n", path.c_str());
        return 1;
    }

    std::vector<std::string> msgs;
    msgs.reserve(50000);
    for (std::string line; std::getline(in, line);) {
        if (!line.empty()) {
            msgs.push_back(std::move(line));
            if (msgs.size() >= 50000) break;
        }
    }

    if (msgs.empty()) {
        std::puts("[BENCH] Error: Input corpus is empty. Nothing to benchmark.");
        return 1;
    }

    std::size_t totalBytes = 0;
    for (const auto& m : msgs) {
        totalBytes += m.size();
    }

    razobill::core::SlotRecord slots;
    std::string formKey;
    std::string reconstructed;
    char buffer[1024];

    auto timeOperation = [&](const char* operationName, auto&& benchmarkLambda) {
        double bestSeconds = 1e300;
        std::size_t sink = 0;

        for (int rep = 0; rep < 7; ++rep) {
            const auto t0 = std::chrono::steady_clock::now();
            for (const auto& m : msgs) {
                sink += benchmarkLambda(m);
            }
            const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            bestSeconds = std::min(bestSeconds, elapsed);
        }

        const double totalMsgs = static_cast<double>(msgs.size());
        const double nsPerMsg = (bestSeconds / totalMsgs) * 1e9;
        const double mbPerSec = (static_cast<double>(totalBytes) / bestSeconds) / 1e6;
        const double megaMsgsPerSec = (totalMsgs / bestSeconds) / 1e6;

        std::printf("%-40s %8.1f ns/msg  %7.1f MB/s  %6.2f M msg/s  (sink %zu)\n", operationName, nsPerMsg, mbPerSec,
                    megaMsgsPerSec, sink % 7);
    };

    std::printf(
        "===================================================================================================\n");
    std::printf("RAZOBILL SINGLE-CORE COMPREHENSIVE BENCHMARK (C++20, C ABI & HARDWARE SIMD)\n");
    std::printf(
        "===================================================================================================\n");
    std::printf("Corpus Dataset : %s\n", path.c_str());
    std::printf("Sample Events  : %zu\n", msgs.size());
    std::printf("Total Payload  : %zu bytes (%.2f MB)\n", totalBytes, static_cast<double>(totalBytes) / 1e6);
    std::printf("Mean Msg Length: %.1f bytes/msg\n",
                static_cast<double>(totalBytes) / static_cast<double>(msgs.size()));
    std::printf(
        "---------------------------------------------------------------------------------------------------\n");

    // 1. C++20 Core Lexical Parsing
    timeOperation("C++20 parse (form + slots)", [&](const std::string& m) {
        (void)razobill::core::parse(m, slots);
        return slots.ints.size() + slots.strs.size();
    });

    // 2. C++20 Parsing + Form Key String Generation
    timeOperation("C++20 parse + formKey", [&](const std::string& m) {
        (void)razobill::core::parse(m, slots);
        slots.formKey(formKey);
        return formKey.size();
    });

    // 3. C++20 Parsing + Form Key + XXH64 Hashing
    timeOperation("C++20 parse + formKey + xxh64", [&](const std::string& m) {
        (void)razobill::core::parse(m, slots);
        slots.formKey(formKey);
        return razobill::core::xxh64(formKey) & 1;
    });

    // 4. C++20 Full Round-Trip (Lossless Invertibility)
    timeOperation("C++20 parse + reconstruct (round-trip)", [&](const std::string& m) {
        (void)razobill::core::parse(m, slots);
        slots.formKey(formKey);
        (void)razobill::core::reconstruct(formKey, slots.ints, slots.strs, reconstructed);
        return reconstructed.size();
    });

    // 5. Universal C ABI: RzParse
    timeOperation("C ABI RzParse", [&](const std::string& m) {
        RzSlotRecord r{};
        (void)RzParse(m.data(), m.size(), RZ_SCAN_SEMANTIC, &r);
        std::size_t count = r.intCount + r.strCount;
        RzFreeRecord(&r);
        return count;
    });

    // 6. Universal C ABI: RzDecompose (Zero-Copy Tuple Extraction)
    timeOperation("C ABI RzDecompose (zero-copy)", [&](const std::string& m) {
        RzSlotRecord r{};
        (void)RzDecompose(m.data(), m.size(), RZ_SCAN_SEMANTIC, &r);
        std::size_t res = r.formKeyLength + r.intCount;
        RzFreeRecord(&r);
        return res;
    });

    // 7. Universal C ABI: RzReconstruct
    timeOperation("C ABI RzReconstruct", [&](const std::string& m) {
        RzSlotRecord r{};
        (void)RzParse(m.data(), m.size(), RZ_SCAN_SEMANTIC, &r);
        std::size_t outLen = 0;
        (void)RzReconstruct(&r, buffer, sizeof(buffer), &outLen);
        RzFreeRecord(&r);
        return outLen;
    });

    // 8. Universal C ABI: RzReconstructSlots (Direct Arrays)
    timeOperation("C ABI RzReconstructSlots", [&](const std::string& m) {
        RzSlotRecord r{};
        (void)RzDecompose(m.data(), m.size(), RZ_SCAN_SEMANTIC, &r);
        std::size_t outLen = 0;
        (void)RzReconstructSlots(r.formKey, r.formKeyLength, r.intValues, r.intCount, r.strValues, r.strLengths,
                                 r.strCount, buffer, sizeof(buffer), &outLen);
        RzFreeRecord(&r);
        return outLen;
    });

    // 9. Hardware SIMD Whitespace Scanner (AVX2 / ARM NEON multi-dispatch)
    timeOperation("SIMD findWhitespace (AVX2 / NEON)",
                  [&](const std::string& m) { return razobill::core::simd::findWhitespace(m.data(), m.size()); });

    // 10. Hardware SIMD Dynamic CPUID Dispatcher
    timeOperation("SIMD findWhitespaceDynamic (CPUID)", [&](const std::string& m) {
        return razobill::core::simd::findWhitespaceDynamic(m.data(), m.size());
    });

    // 11. Scalar Whitespace Scanner Fallback
    timeOperation("Scalar std::isspace delimiter check", [&](const std::string& m) {
        std::size_t idx = 0;
        while (idx < m.size() && !std::isspace(static_cast<unsigned char>(m[idx]))) {
            ++idx;
        }
        return idx;
    });

    // 11. XXH64 Raw Message Hash Theoretical Ceiling
    timeOperation("XXH64 raw message hash baseline",
                  [&](const std::string& m) { return razobill::core::xxh64(m) & 1; });

    std::printf(
        "===================================================================================================\n");
    return 0;
}
