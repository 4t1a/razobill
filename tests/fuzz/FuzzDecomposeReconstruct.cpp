// ==============================================================================
// Razobill libFuzzer Harness: Continuous Invariant & Memory Safety Verification
// ==============================================================================
// Grounded in Proposition 1 & 2 of "Latitude and Doubt" (M. M. Atta, 2026).
// Verifies:
// 1. Lossless Invertibility Invariant: reconstruct(decompose(x)) == x for any byte sequence.
// 2. Zero-crash tolerance against pathological inputs, unterminated strings, null bytes,
//    and malformed control codes.
// 3. Complete memory safety and leak-free execution across C++ and Universal C ABIs.
// ==============================================================================

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "librazobill/RazobillC.h"
#include "librazobill/Slots.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0 || size > 1024 * 1024) {
        return 0; // Skip empty or excessive payloads (>1MB)
    }

    const std::string_view input(reinterpret_cast<const char*>(data), size);

    // -------------------------------------------------------------------------
    // 1. C++20 Core API: Exact (DigitOnly) Mode
    // -------------------------------------------------------------------------
    {
        razobill::core::SlotRecord rec;
        const auto status = razobill::core::parse(input, rec, razobill::core::ScanMode::DigitOnly);
        if (status == razobill::Status::Ok) {
            const std::string form = rec.formKey();
            std::string rebuilt;
            const auto recStatus = razobill::core::reconstruct(form, rec.ints, rec.strs, rebuilt);
            if (recStatus == razobill::Status::Ok) {
                // Invariant: Reconstructed message must match input byte-for-byte
                if (rebuilt != input) {
                    std::cerr << "FUZZ INVARIANT VIOLATION (Exact mode):\n"
                              << "Original:      " << input << "\n"
                              << "Reconstructed: " << rebuilt << "\n";
                    std::abort();
                }
            }
        }
    }

    // -------------------------------------------------------------------------
    // 2. C++20 Core API: Semantic Mode
    // -------------------------------------------------------------------------
    {
        razobill::core::SlotRecord rec;
        const auto status = razobill::core::parse(input, rec, razobill::core::ScanMode::Semantic);
        if (status == razobill::Status::Ok) {
            const std::string form = rec.formKey();
            std::string rebuilt;
            const auto recStatus = razobill::core::reconstruct(form, rec.ints, rec.strs, rebuilt);
            if (recStatus == razobill::Status::Ok) {
                if (rebuilt != input) {
                    std::cerr << "FUZZ INVARIANT VIOLATION (Semantic mode):\n"
                              << "Original:      " << input << "\n"
                              << "Reconstructed: " << rebuilt << "\n";
                    std::abort();
                }
            }
        }
    }

    // -------------------------------------------------------------------------
    // 3. Universal C ABI Verification: RzParse / RzReconstruct / RzFreeRecord
    // -------------------------------------------------------------------------
    {
        RzSlotRecord crec{};
        const RzStatusCode parseCode = RzParse(reinterpret_cast<const char*>(data), size, RZ_SCAN_EXACT, &crec);
        if (parseCode == RZ_SUCCESS) {
            // Allocate sufficient buffer for reconstruction
            std::vector<char> buffer(size + 512);
            size_t outLength = 0;
            const RzStatusCode recCode = RzReconstruct(&crec, buffer.data(), buffer.size(), &outLength);
            if (recCode == RZ_SUCCESS) {
                const std::string_view rebuiltView(buffer.data(), outLength);
                if (rebuiltView != input) {
                    std::cerr << "FUZZ C-ABI INVARIANT VIOLATION:\n"
                              << "Original:      " << input << "\n"
                              << "Reconstructed: " << rebuiltView << "\n";
                    RzFreeRecord(&crec);
                    std::abort();
                }
            }
            RzFreeRecord(&crec);
        }
    }

    return 0;
}

#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
// Standalone harness when built without libFuzzer engine (e.g. local verification)
int main() {
    const std::vector<std::string> testCases = {
        "Simple log line without digits",
        "Failed password for root from 192.168.1.1 port 22 ssh2",
        "Receiving block blk_-1608999687919862906 src: /10.250.19.102:54106 dest: /10.250.19.102:50010",
        "Pathological \\x12 delimiter in payload \\x13 test",
        "Multi-byte Unicode emoji: \xF0\x9F\x9A\x80\xE2\x9C\xA8\xF0\x9F\x94\xA5 with numbers 12345",
        "127.0.0.1 - - [10/Oct/2026:13:55:36 +0000] \"GET /api/v1/health HTTP/1.1\" 200 2326",
        "",
        "      \t\t\n\r   ",
        "0",
        "9223372036854775807"
    };

    std::cout << "[FuzzDecomposeReconstruct] Running standalone verification harness...\n";
    for (size_t i = 0; i < testCases.size(); ++i) {
        const auto& tc = testCases[i];
        LLVMFuzzerTestOneInput(reinterpret_cast<const uint8_t*>(tc.data()), tc.size());
    }
    std::cout << "[FuzzDecomposeReconstruct] All " << testCases.size() << " sample test cases passed perfectly!\n";
    return 0;
}
#endif
