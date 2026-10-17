# Razobill Mathematical & Information-Theoretic Audit

This document audits the mathematical foundations against the source paper:  
*"Latitude and Doubt: An exact ledger of meaning in messages"* (M. M. Atta, 2026)

---

## Theorem 1 — Fundamental Ledger of Information

**Implementation:** [Stats.cpp — `ledger()`](file:///C:/FM/rzd/razobill/src/Stats.cpp)

$$E = I(X;W) + \Lambda, \quad C = I(X;W) + \Delta, \quad E - C = \Lambda - \Delta$$

**Verification:**

```cpp
const double Lambda = std::max(J - C, 0.0);  // H(X|W) = H(X,W) - H(W)
const double Delta  = std::max(J - E, 0.0);  // H(W|X) = H(X,W) - H(X)
const double I      = std::max(C + E - J, 0.0); // I(X;W) = H(X) + H(W) - H(X,W)
```

**Issue T1: Independent Clamping Violation**

Each term is clamped to `max(0)` independently. Due to floating-point rounding, this can violate the identity:

```
E = I + Lambda   →   should hold exactly
C = I + Delta    →   should hold exactly
```

After independent clamping, `I + Lambda` may differ from `E` by small epsilon.

**Fix:** Clamp `J` first, then derive consistently:

```cpp
const double J_clamped = std::max({J, C, E});  // J >= max(C, E) by definition
const double Lambda = J_clamped - C;
const double Delta  = J_clamped - E;
const double I      = C + E - J_clamped;
```

Or: compute `I = max(C + E - J, 0.0)` first, then derive `Lambda = E - I`, `Delta = C - I`.

**Severity:** 🟡 MEDIUM — affects ledger identity verification at near-zero entropy values.

---

## Proposition 10 — Miller-Madow Bias Correction

**Implementation:** [Stats.cpp — `ledger()` lines 190–215](file:///C:/FM/rzd/razobill/src/Stats.cpp#L190)

$$\hat{H}_{MM} = \hat{H} + \frac{K-1}{2n \ln 2}$$

**Verification:** Correctly applies stratified Miller-Madow correction per meaning class:

```cpp
const double mmCondH = (kObs > 1)
    ? rawCondH + (static_cast<double>(kObs - 1) / (wMass * twoLn2))
    : rawCondH;
```

**Status:** ✅ Correct. Uses `twoLn2 = 2 * ln(2) = 1.3862...` as the denominator for base-2 entropy bias correction.

---

## Theorem 8 — Steganographic Channel Capacity

**Implementation:** [Channel.cpp — `analyzeCapacity()`](file:///C:/FM/rzd/razobill/src/Channel.cpp)

$$\mu = \sum_{i=1}^n H(X_i | w_i), \quad \sigma^2 = \sum_{i=1}^n \text{Var}(-\log_2 Q(X_i | w_i))$$

Safe payload: $k = \lfloor \mu - 3\sigma - 7 \rfloor$

**Verification:**

```cpp
const double var = std::max(0.0, secondMoment - (h * h));
mu += h;
sigma2 += var;
// ...
const double rawK = std::floor(mu - 3.0 * sigma - 7.0);
```

**Status:** ✅ Correct. The 3σ guard and 7-bit safety margin match the paper's recommended conservative bound.

---

## Issue T2: IEEE 754 Precision Limit in Channel

**File:** [Channel.cpp:156–163, 262–286](file:///C:/FM/rzd/razobill/src/Channel.cpp#L156)  
**Severity:** 🟡 MEDIUM  

```cpp
const double numCells = std::ldexp(1.0, static_cast<int>(payloadBits)); // 2^k
const double z = (static_cast<double>(message) + clampedDither) / numCells;
```

**Problem:** IEEE 754 `double` has 53 bits of mantissa. When `payloadBits > 53`:
- `static_cast<double>(message)` truncates lower bits of the `uint64_t` message
- `numCells = 2^k` overflows to `+inf` when `k >= 1024`
- Casting `+inf` to `uint64_t` in `decode()` is **undefined behavior**

**Practical impact:** The safe payload formula `k = floor(mu - 3σ - 7)` typically produces k < 40 for realistic sequences, so this is unlikely in practice. But the code should guard against it.

**Fix:**

```cpp
if (payloadBits > 53) {
    res.success = false;
    return res;
}
```

---

## Issue T3: Probability Mass Leakage in `exactFailureProbability()`

**File:** [Channel.cpp:121–128](file:///C:/FM/rzd/razobill/src/Channel.cpp#L121-L128)  
**Severity:** 🟡 MEDIUM  

```cpp
constexpr std::size_t kMaxDistStates = 4096;
if (nextDist.size() > kMaxDistStates) {
    for (auto it = nextDist.begin(); it != nextDist.end();) {
        if (it->second < 1e-12)
            it = nextDist.erase(it);
        else
            ++it;
    }
}
```

**Problem:** Pruning states with probability < 10⁻¹² breaks the conservation of total probability mass: $\sum p_i < 1.0$ after pruning. Over long sequences, this causes **systematic underestimation** of the failure probability.

**Impact:** The function returns a failure probability that is too optimistic. In safety-critical applications (e.g., digital forensics provenance), this could lead to accepting an unsafe payload size.

**Fix:** Track the pruned mass and add it as a correction term:

```cpp
double prunedMass = 0.0;
for (auto it = nextDist.begin(); it != nextDist.end();) {
    if (it->second < 1e-12) {
        prunedMass += it->second;
        it = nextDist.erase(it);
    } else {
        ++it;
    }
}
// ... later ...
return std::min(1.0, std::max(0.0, delta + prunedMass));
```

---

## Issue T4: Circular Boundary Rejection in `decode()`

**File:** [Channel.cpp:279](file:///C:/FM/rzd/razobill/src/Channel.cpp#L279)  
**Severity:** 🟡 MEDIUM  

```cpp
if (leftZ < 0.0 || rightZ >= 1.0) {
    // Interval straddles circular 0/1 boundary
    res.success = false;
    return res;
}
```

**Problem:** The channel operates on the unit circle $[0, 1)$ with key-shift modular arithmetic. When the decoded interval wraps around the 0/1 boundary, the code rejects it as a failure instead of handling the wrap.

**Impact:** False negatives — valid encoded messages are rejected when the interval happens to straddle the boundary. The probability of this depends on the key value and payload size.

**Fix:** Handle the wrap-around case by checking both halves of the split interval:

```cpp
if (leftZ < 0.0) {
    // Interval wraps: check [leftZ+1, 1) ∪ [0, rightZ)
    const auto cellA = static_cast<uint64_t>(std::floor((leftZ + 1.0) * numCells));
    const auto cellB = static_cast<uint64_t>(std::floor(rightZ * numCells));
    if (cellA == cellB) { res.success = true; res.message = cellA; }
    // ...
}
```

---

## Theorem 3 — Semantic Gating Cost (JSD)

**Implementation:** [Stats.cpp — `mergeCost()`](file:///C:/FM/rzd/razobill/src/Stats.cpp)

$$\Delta I = \pi_{A \cup B} \cdot \text{JSD}(P_A, P_B)$$

**Verification:** The implementation computes JSD as:

```cpp
jsd.add((n / N) * std::log2(p_yf / p_yF));
```

This is the KL-divergence-weighted form. With Neumaier compensated summation.

**Status:** ✅ Correct. Matches the volume-weighted Jensen-Shannon divergence formula.

---

## Proposition 6 — Depth Ledger (Telescopic Decomposition)

**Implementation:** [Stats.cpp — `computeDepthLedger()`](file:///C:/FM/rzd/razobill/src/Stats.cpp)

$$H(X) = H(W_L) + \sum_{k=1}^L H(W_{k-1} | W_k)$$

**Verification:** Uses joint/conditional entropy via the chain rule. Correct implementation with multi-level hash maps.

**Status:** ✅ Correct.

---

## Neumaier Compensated Summation

**Implementation:** [Stats.cpp — `Sum` class](file:///C:/FM/rzd/razobill/src/Stats.cpp)

```cpp
class Sum {
    void add(double x) noexcept {
        const double t = s_ + x;
        c_ += std::abs(s_) >= std::abs(x) ? (s_ - t) + x : (x - t) + s_;
        s_ = t;
    }
    double value() const noexcept { return s_ + c_; }
};
```

**Status:** ✅ Textbook Neumaier algorithm. Eliminates catastrophic cancellation. O(ε) error bound. Used consistently throughout all entropy computations.

Reference: A. Neumaier, "Rundungsfehleranalyse einiger Verfahren zur Summation endlicher Summen", 1974.

---

## Summary

| Component | Correctness | Issues |
|-----------|:-----------:|--------|
| Ledger (Theorem 1) | ⚠️ | T1: Independent clamping violates identity |
| Miller-Madow (Prop 10) | ✅ | — |
| Channel Capacity (Thm 8) | ✅ | — |
| Channel Embed/Decode | ⚠️ | T2: precision, T4: wrap-around |
| Exact Failure Prob | ⚠️ | T3: probability mass leakage |
| JSD / Merge Cost (Thm 3) | ✅ | — |
| Depth Ledger (Prop 6) | ✅ | — |
| Compensated Summation | ✅ | — |
