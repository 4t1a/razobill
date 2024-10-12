// Unit tests for Latitude Steganographic / Provenance Channel (Theorem 8, Proposition 9).
// Verifies exact message embedding, lossless decoding, capacity bounds, and semantic invariance.
#include <cmath>
#include <cstdint>
#include <vector>

#include "doctest.h"
#include "librazobill/Channel.hpp"

using namespace razobill::channel;

TEST_CASE("Channel capacity analysis for synonymous sequences") {
    // 100 choices, each with 2 equally likely synonymous forms (1 bit of latitude per choice)
    std::vector<MeaningChoice> sequence;
    sequence.reserve(100);
    for (std::uint32_t i = 0; i < 100; ++i) {
        MeaningChoice choice;
        choice.meaning = i % 10;
        choice.options = {SynonymousOption{0, 0.5}, SynonymousOption{1, 0.5}};
        sequence.push_back(std::move(choice));
    }

    auto cap = analyzeCapacity(sequence);
    CHECK(cap.meanCapacityBits == doctest::Approx(100.0));
    CHECK(cap.variance == doctest::Approx(0.0).epsilon(1e-9));
    // Safe payload k = floor(mu - 3*sigma - 7) = 100 - 7 = 93
    CHECK(cap.safePayloadBits == 93);

    // Sequence with no latitude (deterministic 1 form per meaning)
    std::vector<MeaningChoice> zeroSeq;
    for (std::uint32_t i = 0; i < 50; ++i) {
        MeaningChoice choice;
        choice.meaning = i;
        choice.options = {SynonymousOption{0, 1.0}};
        zeroSeq.push_back(std::move(choice));
    }
    auto zeroCap = analyzeCapacity(zeroSeq);
    CHECK(zeroCap.meanCapacityBits == 0.0);
    CHECK(zeroCap.safePayloadBits == 0);
}

TEST_CASE("Channel embed and decode lossless message recovery") {
    // Sequence of 16 synonymous choices, 4 options each (2 bits per choice = 32 bits capacity)
    std::vector<MeaningChoice> sequence;
    for (std::uint32_t i = 0; i < 16; ++i) {
        MeaningChoice choice;
        choice.meaning = i;
        choice.options = {SynonymousOption{0, 0.25}, SynonymousOption{1, 0.25}, SynonymousOption{2, 0.25},
                          SynonymousOption{3, 0.25}};
        sequence.push_back(std::move(choice));
    }

    const std::uint64_t originalMessage = 0xABCDULL;
    constexpr std::size_t payloadBits = 16;     // within safe capacity bounds
    constexpr double sharedKey = 0.38196601125; // golden ratio fraction

    auto embedRes = embed(sequence, originalMessage, payloadBits, sharedKey, 0.5);
    REQUIRE(embedRes.success);
    CHECK(embedRes.payloadBits == payloadBits);
    CHECK(embedRes.chosenForms.size() == sequence.size());

    // Theorem 8: Chosen forms must be valid indices within each choice's option list
    for (std::size_t i = 0; i < sequence.size(); ++i) {
        CHECK(embedRes.chosenForms[i] < sequence[i].options.size());
    }

    // Decode using the shared key
    auto decodeRes = decode(sequence, embedRes.chosenForms, payloadBits, sharedKey);
    REQUIRE(decodeRes.success);
    CHECK(decodeRes.payloadBits == payloadBits);
    CHECK(decodeRes.message == originalMessage);

    // Decoding with a different key must not decode the original message
    constexpr double wrongKey = 0.71828182845;
    auto wrongDecode = decode(sequence, embedRes.chosenForms, payloadBits, wrongKey);
    if (wrongDecode.success) {
        CHECK(wrongDecode.message != originalMessage);
    }
}

TEST_CASE("Proposition 9: Minimum key length P and Theorem 8 failure probability") {
    // 100 choices, each with 2 equally likely synonymous forms (1 bit max log-prob each)
    std::vector<MeaningChoice> sequence;
    sequence.reserve(100);
    for (std::uint32_t i = 0; i < 100; ++i) {
        MeaningChoice choice;
        choice.meaning = i;
        choice.options = {SynonymousOption{0, 0.5}, SynonymousOption{1, 0.5}};
        sequence.push_back(std::move(choice));
    }

    // Proposition 9: P = ceil(sum max -log2 Q) + s + 1
    // sum max -log2 Q = 100.0
    CHECK(requiredKeyBits(sequence, 64) == 100 + 64 + 1);
    CHECK(requiredKeyBits(sequence, 32) == 100 + 32 + 1);

    auto cap = analyzeCapacity(sequence);
    CHECK(cap.meanCapacityBits == 100.0);

    // Chebyshev bound
    const double bound = failureProbabilityBound(cap, 50, 10.0);
    CHECK(bound >= 0.0);
    CHECK(bound <= 1.0);

    // Exact analytical failure probability on small toy sequence:
    // 2 choices of 2 equal options:
    std::vector<MeaningChoice> toySeq;
    toySeq.push_back(MeaningChoice{0, {SynonymousOption{0, 0.5}, SynonymousOption{1, 0.5}}});
    toySeq.push_back(MeaningChoice{1, {SynonymousOption{0, 0.5}, SynonymousOption{1, 0.5}}});

    // For k = 1 bit: delta = sum_{x^2} Q min{1, 2^1 * Q} = 4 * 0.25 * min{1, 0.5} = 0.5
    CHECK(exactFailureProbability(toySeq, 1) == doctest::Approx(0.5).epsilon(1e-6));

    // For k = 0 bits: delta = sum_{x^2} Q^2 = 4 * 0.0625 = 0.25
    CHECK(exactFailureProbability(toySeq, 0) == doctest::Approx(0.25).epsilon(1e-6));

    // For k = 2 bits: delta = sum_{x^2} Q min{1, 4 * 0.25} = 1.0
    CHECK(exactFailureProbability(toySeq, 2) == doctest::Approx(1.0).epsilon(1e-6));
}
