// Latitude Steganographic & Provenance Channel
// Exact implementation of Theorem 8 (Capacity) and Proposition 9 (Finite-Precision Implementation)
// from "Latitude and Doubt" (M. M. Atta, 2026).
//
// In an unambiguous semantic source (Delta = 0), Latitude Lambda = H(X | W) is an operational channel:
// exactly the number of bits per symbol that can be written into the form without changing the
// meaning or the statistics of the text.
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace razobill::channel {

// Candidate synonymous choice for a given meaning
struct SynonymousOption {
    std::size_t formIndex; // Identifier / index of candidate form
    double probability;    // P(form | meaning) > 0
};

// Stratum of synonymous forms for a single meaning step
struct MeaningChoice {
    std::uint32_t meaning;
    std::vector<SynonymousOption> options; // sorted by formIndex, sum(probability) == 1.0
};

// Theoretical channel metrics for a meaning sequence (Theorem 8)
struct ChannelCapacity {
    double meanCapacityBits = 0.0;   // mu = sum_i H(X | w_i)
    double variance = 0.0;           // sigma^2 = sum_i Var[-log2 P(X | w_i)]
    std::size_t safePayloadBits = 0; // k = floor(mu - 3*sigma - 7)
};

// Result of message embedding into synonymous choices
struct EmbedResult {
    bool success = false;
    std::size_t payloadBits = 0;
    std::uint64_t embeddedMessage = 0;
    std::vector<std::size_t> chosenForms; // index of chosen form for each step
};

// Result of message extraction from observed forms
struct DecodeResult {
    bool success = false;
    std::size_t payloadBits = 0;
    std::uint64_t message = 0;
};

// Computes capacity mu, variance sigma^2, and safe payload k for a sequence of choices
[[nodiscard]] ChannelCapacity analyzeCapacity(std::span<const MeaningChoice> sequence) noexcept;

// Proposition 9: Finite-precision key length bound.
// Returns minimum key length P in bits:
// P = ceil(sum_i max_x (-log2 Q(x | w_i))) + s + 1
// Guarantees variation distance <= 2^(-s-1) and failure prob within 2^(-s) + 2^(k+1-P) of ideal.
[[nodiscard]] std::size_t requiredKeyBits(std::span<const MeaningChoice> sequence, std::size_t s = 64) noexcept;

// Chebyshev upper bound on failure probability for payload k and safety margin a (Theorem 8):
// delta <= sigma^2 / a^2 + 2^-s where a = max(1.0, mu - k - s)
[[nodiscard]] double failureProbabilityBound(const ChannelCapacity& capacity, std::size_t payloadBits,
                                             double s = 64.0) noexcept;

// Exact analytical failure probability: delta = sum_{x^n} Q(x^n | w^n) min{1, 2^k Q(x^n | w^n)} (Theorem 8).
// Evaluated via exact distribution convolution across independent synonymous positions.
[[nodiscard]] double exactFailureProbability(std::span<const MeaningChoice> sequence, std::size_t payloadBits) noexcept;

// Embeds an integer message m into synonymous forms using shared key R and dither V.
// Follows the arithmetic cumulative interval scheme of Theorem 8 and Proposition 9.
[[nodiscard]] EmbedResult embed(std::span<const MeaningChoice> sequence, std::uint64_t message, std::size_t payloadBits,
                                double sharedKey, double dither = 0.5);

// Decodes the embedded message from the observed forms using shared key R.
// Returns success = false if the interval crosses cell boundaries. Never returns a corrupted payload.
[[nodiscard]] DecodeResult decode(std::span<const MeaningChoice> sequence, std::span<const std::size_t> observedForms,
                                  std::size_t payloadBits, double sharedKey);

} // namespace razobill::channel
