// ==============================================================================
// Razobill libFuzzer Harness: Continuous Invariant & Memory Safety Verification
// ==============================================================================

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "razobill.h"
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
        if (status != razobill::Status::Ok) {
            std::cerr << "FUZZ ERROR: parse failed in Exact mode with status "
                      << static_cast<int>(status) << "\n";
            std::abort();
        }
        const std::string form = rec.formKey();
        std::string rebuilt;
        const auto recStatus = razobill::core::reconstruct(form, rec.ints, rec.strs, rebuilt);
        if (recStatus != razobill::Status::Ok) {
            std::cerr << "FUZZ ERROR: reconstruct failed in Exact mode with status "
                      << static_cast<int>(recStatus) << "\n";
            std::abort();
        }
        if (rebuilt != input) {
            std::cerr << "FUZZ INVARIANT VIOLATION (Exact mode):\n"
                      << "Original:      " << input << "\n"
                      << "Reconstructed: " << rebuilt << "\n";
            std::abort();
        }
    }

    // -------------------------------------------------------------------------
    // 2. C++20 Core API: Semantic Mode
    // -------------------------------------------------------------------------
    {
        razobill::core::SlotRecord rec;
        const auto status = razobill::core::parse(input, rec, razobill::core::ScanMode::Semantic);
        if (status != razobill::Status::Ok) {
            std::cerr << "FUZZ ERROR: parse failed in Semantic mode with status "
                      << static_cast<int>(status) << "\n";
            std::abort();
        }
        const std::string form = rec.formKey();
        std::string rebuilt;
        const auto recStatus = razobill::core::reconstruct(form, rec.ints, rec.strs, rebuilt);
        if (recStatus != razobill::Status::Ok) {
            std::cerr << "FUZZ ERROR: reconstruct failed in Semantic mode with status "
                      << static_cast<int>(recStatus) << "\n";
            std::abort();
        }
        if (rebuilt != input) {
            std::cerr << "FUZZ INVARIANT VIOLATION (Semantic mode):\n"
                      << "Original:      " << input << "\n"
                      << "Reconstructed: " << rebuilt << "\n";
            std::abort();
        }
    }

    // -------------------------------------------------------------------------
    // 3. Universal C ABI Verification: razobill_split / razobill_join / razobill_free
    // -------------------------------------------------------------------------
    {
        razobill_record_t crec{};
        const razobill_status_t splitCode = razobill_split(reinterpret_cast<const char*>(data), size, &crec);
        if (splitCode != RAZOBILL_OK) {
            std::cerr << "FUZZ ERROR: C-ABI razobill_split failed with status " << splitCode << "\n";
            std::abort();
        }

        // Buffer probing
        size_t requiredLen = 0;
        const razobill_status_t probeCode = razobill_join(&crec, nullptr, 0, &requiredLen);
        if (probeCode != RAZOBILL_ERR_BUFFER_TOO_SMALL || requiredLen != size) {
            std::cerr << "FUZZ ERROR: C-ABI razobill_join probing failed\n";
            razobill_free(&crec);
            std::abort();
        }

        // Allocate exact buffer
        std::vector<char> buffer(size);
        size_t outLength = 0;
        const razobill_status_t joinCode = razobill_join(&crec, buffer.data(), buffer.size(), &outLength);
        if (joinCode != RAZOBILL_OK) {
            std::cerr << "FUZZ ERROR: C-ABI razobill_join failed with status " << joinCode << "\n";
            razobill_free(&crec);
            std::abort();
        }

        const std::string_view rebuiltView(buffer.data(), outLength);
        if (rebuiltView != input) {
            std::cerr << "FUZZ C-ABI INVARIANT VIOLATION:\n"
                      << "Original:      " << input << "\n"
                      << "Reconstructed: " << rebuiltView << "\n";
            razobill_free(&crec);
            std::abort();
        }
        razobill_free(&crec);
    }

    return 0;
}

#if !defined(LIBRAZOBILL_USE_LIBFUZZER) && !defined(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION)
int main(int /*argc*/, char** /*argv*/) {
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
