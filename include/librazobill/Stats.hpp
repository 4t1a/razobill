// Information-theoretic ledger, leakage, divergence, and consequence decomposition
// grounded in "Latitude and Doubt" (M. M. Atta, 2026).
//
// CONSERVATION LAWS & IDENTITIES (Theorems 1, 4, 5, Corollary 2, Proposition 3, 6, 10):
//     E = I + Lambda     (Expression = Conveyance + Latitude)
//     C = I + Delta      (Content    = Conveyance + Doubt)
//     E - C = Lambda - Delta
//     d(W, X) = Lambda + Delta = H(W, X) - I(W; X)   [Proposition 3 distance]
//     E = I(X; Y) + H(M | Y) + H(X | M)             [Theorem 4c: Consequence + Hedge + Latitude]
//     2^E <= N, Delta - Lambda = C - E >= C - log2(N) [Corollary 2: Vocabulary Law]
#pragma once

#include <cmath>
#include <cstdint>
#include <span>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace razobill::stats {

// Joint mass of (form x, meaning w): count of events times p(w|x).
struct Cell {
    std::uint64_t form;
    std::uint32_t meaning;
    double mass;
};

// Events with form x and purpose value y.
struct XY {
    std::uint64_t form;
    std::uint32_t y;
    double n;
};

// Form to meaning probabilistic map.
struct FormMeaning {
    std::uint64_t form;
    std::uint32_t meaning;
    double p;
};

// Ledger of a semantic source (Theorems 1 & 4 in "Latitude and Doubt")
struct Ledger {
    double coverage = 0.0;   // resolved events / all events (fraction in [0, 1])
    double content = 0.0;    // C = H(W): what is meant (entropy of meanings)
    double expression = 0.0; // E = H(X): what is said (entropy of forms)
    double latitude = 0.0;   // Lambda = H(X|W): freedom of the speaker once meaning is fixed
    double doubt = 0.0;      // Delta = H(W|X): what the listener cannot recover from the form
    double conveyance = 0.0; // I = I(W;X): what the form conveys about meaning
    double distance = 0.0;   // d(W, X) = Lambda + Delta = H(W, X) - I (Proposition 3)

    // Effective vocabulary (perplexity) measures: 2^H (Section 2.1 & Corollary 2)
    double effectiveVocabulary = 0.0; // 2^E: effective number of forms
    double effectiveContent = 0.0;    // 2^C: effective number of meanings
    double effectiveLatitude = 0.0;   // 2^Lambda: effective number of ways a meaning is expressed
    double effectiveDoubt = 0.0;      // 2^Delta: effective number of meanings a form is taken to have

    // Stratified Miller-Madow bias-corrected estimators (Section 5.1)
    double contentMm = 0.0;    // H_MM(W)
    double expressionMm = 0.0; // H_MM(X)
    double latitudeMm = 0.0;   // Stratified H_MM(X|W)
    double doubtMm = 0.0;      // Stratified H_MM(W|X)
    double conveyanceMm = 0.0; // I_MM(W; X) = C_MM - Delta_MM

    // Proposition 10 certified confidence interval for small meaning alphabet
    double contentEpsilon = 0.0;
    double contentHalfWidth = 0.0;

    // Checks whether the fundamental ledger identities hold within numerical tolerance
    [[nodiscard]] bool satisfiesLedgerIdentities(double tol = 1e-9) const noexcept {
        const double diffE = std::abs(expression - (conveyance + latitude));
        const double diffC = std::abs(content - (conveyance + doubt));
        const double diffBal = std::abs((expression - content) - (latitude - doubt));
        return diffE <= tol && diffC <= tol && diffBal <= tol && latitude >= -tol && doubt >= -tol &&
               conveyance >= -tol;
    }

    // Checks the Vocabulary Law bound: Delta - Lambda >= C - log2(N)
    [[nodiscard]] bool satisfiesVocabularyLaw(std::size_t nDistinctForms, double tol = 1e-9) const noexcept {
        if (nDistinctForms == 0) return true;
        const double maxE = std::log2(static_cast<double>(nDistinctForms));
        return (doubt - latitude) >= (content - maxE - tol);
    }
};

// Theorem 4c: Three-way decomposition of form entropy for purpose Y
// E = I(X; Y) [consequence] + H(M | Y) [hedge] + H(X | M) [latitude]
struct ConsequenceDecomposition {
    double expression = 0.0;  // H(X)
    double consequence = 0.0; // I(X; Y): information about purpose Y carried by the message
    double hedge = 0.0;       // H(M | Y): entropy of receiver's confidence not explained by truth
    double latitude = 0.0;    // H(X | M): variation of form to which the purpose is blind

    [[nodiscard]] bool satisfiesIdentity(double tol = 1e-9) const noexcept {
        return std::abs(expression - (consequence + hedge + latitude)) <= tol;
    }
};

// Theorem 7: The Price of Saying (Section 4.1)
// n(E + Delta) = nC + nLambda: saying instead of meaning costs n*Lambda bits.
struct PriceOfSaying {
    double expectedMeaningBits = 0.0;       // n * C: cost to send meaning directly
    double expectedExpressionBits = 0.0;    // n * E: cost to send form
    double expectedClarificationBits = 0.0; // n * Delta: cost of questions to fix meaning
    double totalSayingBits = 0.0;           // n * (E + Delta) == n * (C + Lambda)
    double sayingCostBits = 0.0;            // n * Lambda: premium of saying instead of meaning
    double lowerBoundBits = 0.0;            // n * (E + Delta)
    double upperBoundBits = 0.0;            // n * (E + Delta) + 2

    [[nodiscard]] bool satisfiesIdentity(double tol = 1e-9) const noexcept {
        const double diff1 = std::abs(totalSayingBits - (expectedMeaningBits + sayingCostBits));
        const double diff2 = std::abs(totalSayingBits - (expectedExpressionBits + expectedClarificationBits));
        return diff1 <= tol && diff2 <= tol;
    }
};

// Computes prefix-code expected length bounds and wording premium (Theorem 7).
[[nodiscard]] PriceOfSaying priceOfSaying(const Ledger& ledger, double nMessages) noexcept;

// Event step for multi-level depth decomposition (Proposition 6)
struct HierarchicalStep {
    std::uint64_t form = 0;            // W_0 = X
    std::vector<std::uint32_t> levels; // W_1, W_2, ..., W_L
    std::uint32_t y = 0;               // Purpose Y
    double mass = 0.0;
};

// Proposition 6: Multi-level Depth Ledger
struct DepthLedger {
    std::vector<double> entropyLevels;   // H(W_k-1 | W_k) from level 1 to L
    std::vector<double> relevanceLevels; // I(W_k-1; Y | W_k) from level 1 to L
    double totalEntropy = 0.0;           // H(X)
    double totalRelevance = 0.0;         // I(X; Y)
    double coarsestEntropy = 0.0;        // H(W_L)
    double coarsestRelevance = 0.0;      // I(W_L; Y)

    // Checks Proposition 6 telescopic identities:
    // H(X) = H(W_L) + sum H(W_k-1 | W_k)
    // I(X; Y) = I(W_L; Y) + sum I(W_k-1; Y | W_k)
    [[nodiscard]] bool satisfiesIdentity(double tol = 1e-9) const noexcept {
        double sumH = coarsestEntropy;
        for (double h : entropyLevels)
            sumH += h;
        double sumI = coarsestRelevance;
        for (double r : relevanceLevels)
            sumI += r;
        return std::abs(totalEntropy - sumH) <= tol && std::abs(totalRelevance - sumI) <= tol;
    }
};

// Computes multi-level depth ledger across successive functional projections (Proposition 6).
[[nodiscard]] DepthLedger computeDepthLedger(std::span<const HierarchicalStep> steps);

// Computes Shannon entropy in bits (log base 2) with Neumaier floating-point compensation.
[[nodiscard]] double entropyOfCounts(std::span<const double> counts) noexcept;

// Legacy alias
[[nodiscard]] inline double entropy_of_counts(std::span<const double> counts) noexcept {
    return entropyOfCounts(counts);
}

// Computes the full ledger (Theorem 1, Proposition 3, Corollary 2, Section 5.1).
[[nodiscard]] Ledger ledger(std::span<const Cell> cells, double totalEvents, double confidenceAlpha = 0.05);

// Theorem 4c consequence decomposition calculation.
[[nodiscard]] ConsequenceDecomposition consequenceDecomposition(std::span<const XY> xy,
                                                                std::span<const FormMeaning> map);

// Volume-weighted Jensen-Shannon divergence of P(Y|f) over merged forms (Theorem 4 & Gate).
// share * jsd = I(X;Y) - I(W;Y) for the merge.
struct MergeCost {
    double jsd;
    double share;
};

[[nodiscard]] MergeCost mergeCost(std::span<const XY> xy, const std::unordered_map<std::uint64_t, bool>& merged,
                                  double totalEvents);

// Legacy alias
[[nodiscard]] inline MergeCost merge_cost(std::span<const XY> xy, const std::unordered_map<std::uint64_t, bool>& merged,
                                          double totalEvents) {
    return mergeCost(xy, merged, totalEvents);
}

// Leakage: I(X;Y|W) = H(X,W) + H(Y,W) - H(W) - H(X,Y,W). Zero means W is adequate for Y (Theorem 5).
[[nodiscard]] double leakage(std::span<const XY> xy, std::span<const FormMeaning> map);

// Mutual information I(X;Y) of raw count distributions
[[nodiscard]] double mutualInformation(std::span<const XY> xy);

// Legacy alias
[[nodiscard]] inline double mutual_information(std::span<const XY> xy) {
    return mutualInformation(xy);
}

// Miller-Madow bias correction (Section 5.1): H_MM = H_plugin + (K_obs - 1)/(2n ln 2)
[[nodiscard]] inline double millerMadow(double pluginH, std::size_t observedCategories, double sampleCount) noexcept {
    if (sampleCount <= 0 || observedCategories <= 1) return pluginH;
    constexpr double twoLn2 = 1.3862943611198906;
    return pluginH + static_cast<double>(observedCategories - 1) / (sampleCount * twoLn2);
}

// Proposition 10: Certified interval for small alphabets at significance delta (e.g., delta=0.05)
struct CertifiedInterval {
    double epsilon;
    double halfWidth;
};

[[nodiscard]] inline CertifiedInterval certifiedInterval(std::size_t kCategories, double sampleCount,
                                                         double delta = 0.05) noexcept {
    if (sampleCount <= 0 || kCategories < 2) return {0.0, 0.0};
    constexpr double ln2 = 0.6931471805599453;
    const double eps =
        std::sqrt((2.0 / sampleCount) * (static_cast<double>(kCategories) * ln2 + std::log(1.0 / delta)));
    if (eps >= 0.5) {
        return {eps, std::log2(static_cast<double>(kCategories))};
    }
    const double hw = eps * std::log2(static_cast<double>(kCategories) / eps);
    return {eps, hw};
}

} // namespace razobill::stats
