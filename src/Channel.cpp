// ==============================================================================
// Razobill Latitude Steganographic Channel: Exact Provenance Embedding Engine
// ==============================================================================
//
// THEORETICAL DERIVATION (Atta, 2026):
// 1. Theorem 8 (Operational Channel Capacity):
//    In an unambiguous source ($\Delta = 0$), the mean capacity of a message sequence is:
//    $$\mu = \sum_{i=1}^n H(X_i \mid w_i), \quad \sigma^2 = \sum_{i=1}^n \operatorname{Var}(-\log_2 Q(X_i \mid w_i))$$
//    Safe payload is bounded by $k = \lfloor \mu - 3\sigma - 7 \rfloor$.
//
// 2. Theorem 8 (Analytical Failure Probability):
//    The exact failure probability $\delta = \sum_{x^n} Q(x^n \mid w^n) \min\{1, 2^k Q(x^n \mid w^n)\}$
//    is computed via $O(n \cdot M)$ discrete probability mass convolution over log-likelihoods.
//
// 3. Proposition 9 (Finite-Precision Grid Length):
//    The minimum grid parameter required to ensure exact decodability is:
//    $$P = \left\lceil \sum_{i=1}^n \max_{x \in \mathcal{X}(w_i)} (-\log_2 Q(x \mid w_i)) \right\rceil + s + 1$$
// ==============================================================================

#include "librazobill/Channel.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <unordered_map>

namespace razobill::channel {

ChannelCapacity analyzeCapacity(std::span<const MeaningChoice> sequence) noexcept {
    double mu = 0.0;
    double sigma2 = 0.0;

    for (const auto& step : sequence) {
        if (step.options.empty()) continue;

        double sumP = 0.0;
        for (const auto& opt : step.options)
            sumP += opt.probability;
        if (sumP <= 0.0) continue;

        double h = 0.0;
        double secondMoment = 0.0;
        for (const auto& opt : step.options) {
            if (opt.probability > 0.0) {
                const double p = opt.probability / sumP;
                const double log2p = std::log2(p);
                h += -p * log2p;
                secondMoment += p * (log2p * log2p);
            }
        }
        const double var = std::max(0.0, secondMoment - (h * h));
        mu += h;
        sigma2 += var;
    }

    const double sigma = std::sqrt(sigma2);
    const double rawK = std::floor(mu - (3.0 * sigma) - 7.0);
    const std::size_t k = rawK > 0.0 ? static_cast<std::size_t>(rawK) : 0;

    return {.meanCapacityBits = mu, .variance = sigma2, .safePayloadBits = k};
}

std::size_t requiredKeyBits(std::span<const MeaningChoice> sequence, std::size_t s) noexcept {
    double sumMaxL = 0.0;
    for (const auto& step : sequence) {
        if (step.options.empty()) continue;
        double sumP = 0.0;
        for (const auto& opt : step.options)
            sumP += opt.probability;
        if (sumP <= 0.0) continue;

        double minP = 1.0;
        for (const auto& opt : step.options) {
            if (opt.probability > 0.0) {
                const double p = opt.probability / sumP;
                minP = std::min(minP, p);
            }
        }
        if (minP > 0.0) {
            sumMaxL += -std::log2(minP);
        }
    }
    return static_cast<std::size_t>(std::ceil(sumMaxL)) + s + 1;
}

double failureProbabilityBound(const ChannelCapacity& capacity, std::size_t payloadBits, double s) noexcept {
    const double a = capacity.meanCapacityBits - static_cast<double>(payloadBits) - s;
    if (a <= 0.0) return 1.0;
    const double chebyshev = capacity.variance / (a * a);
    const double slack = std::ldexp(1.0, -static_cast<int>(std::min(s, 100.0)));
    return std::min(1.0, std::max(0.0, chebyshev + slack));
}

// Exact analytical failure probability on discrete distributions via dynamic programming.
//
// Quantization & Truncation Bounds (Theorem 8 / Proposition 9):
// Log-probabilities (-log2 p) are quantized to 0.001-bit resolution (kScale = 1000.0).
// For any sequence of length L, the maximum cumulative quantization drift is bounded
// by at most 0.5 * L / kScale bits.
// State pruning discards low-probability states (mass < 1e-12) once the state space
// exceeds kMaxDistStates (4096). Pruned mass is strictly accumulated in `prunedMass` and added
// back to the final error probability, guaranteeing that the returned delta is a rigorous upper bound.
double exactFailureProbability(std::span<const MeaningChoice> sequence, std::size_t payloadBits) noexcept {
    if (sequence.empty()) return 0.0;

    try {
        constexpr double kScale = 1000.0;
        std::unordered_map<std::int64_t, double> dist{{0, 1.0}};
        double prunedMass = 0.0;

        for (const auto& step : sequence) {
            if (step.options.empty()) continue;
            double sumP = 0.0;
            for (const auto& opt : step.options)
                sumP += opt.probability;
            if (sumP <= 0.0) continue;

            std::unordered_map<std::int64_t, double> nextDist;
            nextDist.reserve(dist.size() * step.options.size());

            for (const auto& opt : step.options) {
                if (opt.probability <= 0.0) continue;
                const double p = opt.probability / sumP;
                const double logP = -std::log2(p);
                const auto shift = static_cast<std::int64_t>(std::round(logP * kScale));

                for (const auto& [bin, prob] : dist) {
                    nextDist[bin + shift] += prob * p;
                }
            }
            constexpr std::size_t kMaxDistStates = 4096;
            if (nextDist.size() > kMaxDistStates) {
                for (auto it = nextDist.begin(); it != nextDist.end();) {
                    if (it->second < 1e-12) {
                        prunedMass += it->second;
                        it = nextDist.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
            dist = std::move(nextDist);
        }

        double delta = 0.0;
        const double twoK = std::ldexp(1.0, static_cast<int>(std::min(payloadBits, std::size_t{1023})));

        for (const auto& [bin, prob] : dist) {
            const double logQ = static_cast<double>(bin) / kScale;
            const double q = std::pow(2.0, -logQ);
            const double minFactor = std::min(1.0, twoK * q);
            delta += prob * minFactor;
        }

        return std::min(1.0, std::max(0.0, delta + prunedMass));
    } catch (...) {
        return 1.0;
    }
}

EmbedResult embed(std::span<const MeaningChoice> sequence, std::uint64_t message, std::size_t payloadBits,
                  double sharedKey, double dither) {
    EmbedResult res;
    res.payloadBits = payloadBits;
    res.embeddedMessage = message;

    if (payloadBits == 0 || payloadBits > 53 || sequence.empty()) {
        res.success = false;
        return res;
    }

    const double numCells = std::ldexp(1.0, static_cast<int>(payloadBits)); // 2^k
    if (static_cast<double>(message) >= numCells) {
        res.success = false;
        return res;
    }

    const double clampedDither = std::clamp(dither, 0.0001, 0.9999);
    const double z = (static_cast<double>(message) + clampedDither) / numCells;
    double u = std::fmod(z + sharedKey, 1.0);
    if (u < 0.0) u += 1.0;

    double curL = 0.0;
    double curR = 1.0;
    res.chosenForms.reserve(sequence.size());

    for (const auto& step : sequence) {
        if (step.options.empty()) {
            res.success = false;
            return res;
        }

        double sumP = 0.0;
        for (const auto& opt : step.options)
            sumP += opt.probability;
        if (sumP <= 0.0) {
            res.success = false;
            return res;
        }

        const double curWidth = curR - curL;
        if (curWidth < 1e-15) {
            res.success = false;
            return res;
        }
        double subL = curL;
        bool found = false;

        for (const auto& opt : step.options) {
            const double p = opt.probability / sumP;
            const double subR = subL + (curWidth * p);

            if (u >= subL && (u < subR || &opt == &step.options.back())) {
                res.chosenForms.push_back(opt.formIndex);
                curL = subL;
                curR = subR;
                found = true;
                break;
            }
            subL = subR;
        }

        if (!found) {
            // Guard edge precision
            res.chosenForms.push_back(step.options.back().formIndex);
        }
    }

    res.success = (res.chosenForms.size() == sequence.size());
    return res;
}

DecodeResult decode(std::span<const MeaningChoice> sequence, std::span<const std::size_t> observedForms,
                    std::size_t payloadBits, double sharedKey) {
    DecodeResult res;
    res.payloadBits = payloadBits;

    if (payloadBits == 0 || payloadBits > 53 || sequence.empty() || observedForms.size() != sequence.size()) {
        res.success = false;
        return res;
    }

    double curL = 0.0;
    double curR = 1.0;

    for (std::size_t i = 0; i < sequence.size(); ++i) {
        const auto& step = sequence[i];
        const auto observedForm = observedForms[i];

        double sumP = 0.0;
        for (const auto& opt : step.options)
            sumP += opt.probability;
        if (sumP <= 0.0) {
            res.success = false;
            return res;
        }

        const double curWidth = curR - curL;
        if (curWidth < 1e-15) {
            res.success = false;
            return res;
        }
        double subL = curL;
        bool match = false;

        for (const auto& opt : step.options) {
            const double p = opt.probability / sumP;
            const double subR = subL + (curWidth * p);

            if (opt.formIndex == observedForm) {
                curL = subL;
                curR = subR;
                match = true;
                break;
            }
            subL = subR;
        }

        if (!match) {
            res.success = false;
            return res;
        }
    }

    const double q = curR - curL;
    const double numCells = std::ldexp(1.0, static_cast<int>(payloadBits)); // 2^k
    const double cellWidth = 1.0 / numCells;

    if (q < 1e-15 || q >= cellWidth) {
        res.success = false;
        return res;
    }

    // Unshift key around interval center: arc A = [centerZ - q/2, centerZ + q/2]
    const double center = (curL + curR) * 0.5;
    double centerZ = center - sharedKey;
    centerZ = centerZ - std::floor(centerZ); // canonical in [0, 1)

    const double halfQ = q * 0.5;
    const double leftZ = centerZ - halfQ;
    const double rightZ = centerZ + halfQ;

    // Handle circular modular interval wrap-around on S^1 = [0, 1) modulo 1
    const double effectiveLeft = (leftZ < 0.0 && leftZ >= -1e-12) ? 0.0 : leftZ;
    const double effectiveRight = (rightZ >= 1.0 && rightZ <= 1.0 + 1e-12) ? std::nextafter(1.0, 0.0) : rightZ;

    const double leftWrapped = effectiveLeft - std::floor(effectiveLeft);
    const double rightWrapped = effectiveRight - std::floor(effectiveRight);

    const auto cellLeft = static_cast<std::uint64_t>(std::floor(leftWrapped * numCells));
    const auto cellRight = static_cast<std::uint64_t>(std::floor(rightWrapped * numCells));

    // Valid if and only if the arc does not cross a cell boundary
    if (cellLeft == cellRight && cellLeft < static_cast<std::uint64_t>(numCells)) {
        res.success = true;
        res.message = cellLeft;
    } else {
        res.success = false;
    }

    return res;
}

} // namespace razobill::channel
