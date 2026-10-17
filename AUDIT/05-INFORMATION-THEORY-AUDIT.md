# 05 — Information Theory & Math Audit

## Scope

Stats.hpp/cpp, Channel.hpp/cpp — mathematical correctness, numerical stability, theorem compliance

---

## Stats.cpp — Ledger & Information Theory

### Neumaier Compensated Summation

#### ⚪ F-MATH-01: Neumaier `Sum` class is correctly implemented

**File**: [`Stats.cpp`](../src/Stats.cpp#L55-L66)

```cpp
void add(double x) noexcept {
    const double t = s_ + x;
    c_ += std::abs(s_) >= std::abs(x) ? (s_ - t) + x : (x - t) + s_;
    s_ = t;
}
```

This is the standard Neumaier (1974) improvement over Kahan summation. The branch handles both cases where `|s_| >= |x|` and `|x| > |s_|`. Under IEEE 754, this guarantees O(ε) total error regardless of summation order. **Mathematically correct**.

---

### Ledger Function (Theorem 1)

#### 🟡 F-MATH-02: Joint entropy `J` is replaced by `J_bound` — undocumented clamping

**File**: [`Stats.cpp`](../src/Stats.cpp#L178-L182)

```cpp
const double J_bound = std::max(std::max(C, E), std::min(J, C + E));
const double Lambda = J_bound - C;
const double Delta = J_bound - E;
const double I = C + E - J_bound;
```

The code computes `J_bound = max(max(C, E), min(J, C + E))`. This clamps the joint entropy to satisfy:
- `J >= C` and `J >= E` (joint entropy >= marginal entropies)
- `J <= C + E` (subadditivity)

These are information-theoretic axioms that should hold for exact distributions but may be violated under finite-sample estimation due to floating-point error. The clamping ensures `Lambda >= 0`, `Delta >= 0`, and `I >= 0` which are necessary for the ledger identities.

**Severity**: 🟡 MEDIUM — correct engineering but should be documented as finite-precision guard
**Fix**: Add a comment explaining why `J_bound` is used instead of raw `J`.

---

#### ⚪ F-MATH-03: Miller-Madow bias correction is correctly implemented

**File**: [`Stats.hpp`](../include/librazobill/Stats.hpp#L198-L202)

```cpp
return pluginH + static_cast<double>(observedCategories - 1) / (sampleCount * twoLn2);
```

This is the standard Miller-Madow (1955) formula: `H_MM = H_plugin + (K-1)/(2n ln 2)`. The constant `twoLn2 = 2 * ln(2) = 1.386294...` is correct. **Mathematically correct**.

---

#### ⚪ F-MATH-04: Certified interval (Proposition 10) is correctly implemented

**File**: [`Stats.hpp`](../include/librazobill/Stats.hpp#L210-L221)

Uses the Hoeffding-style bound:
```
ε = sqrt((2/n) * (K * ln2 + ln(1/δ)))
hw = ε * log2(K/ε)
```

This matches the paper's Proposition 10 formulation. The `eps >= 0.5` guard returns the maximum entropy `log2(K)` as the half-width, which is correct (the bound becomes vacuous).

---

### Merge Cost (Theorem 3 Gate)

#### 🟡 F-MATH-05: `mergeCost` computes KL divergence, not true JSD

**File**: [`Stats.cpp`](../src/Stats.cpp#L306-L332)

```cpp
for (const auto& [key, n] : nfy) {
    const double p_yf = n / nf[key.first];    // P(Y|f)
    const double p_yF = ny[key.second] / N;   // P(Y|F) where F is the merged cluster
    jsd.add((n / N) * std::log2(p_yf / p_yF));
}
```

This computes:
$$\sum_{f \in F} \sum_{y} \frac{n(f,y)}{N} \log_2 \frac{P(y|f)}{P(y|F)}$$

Which is exactly `I(X; Y | X ∈ F)` — the mutual information between the form identity and the purpose Y within the merged cluster. This equals the volume-weighted Jensen-Shannon divergence as stated in Theorem 3. **Mathematically correct**.

---

### Leakage (Theorem 5)

#### ⚪ F-MATH-06: Leakage formula uses joint entropy decomposition

**File**: [`Stats.cpp`](../src/Stats.cpp#L334-L356)

```cpp
return std::max(entropyOfMap(xw) + entropyOfMap(yw) - entropyOfMap(w) - entropyOfMap(xyw), 0.0);
```

This computes `I(X; Y | W) = H(X,W) + H(Y,W) - H(W) - H(X,Y,W)` which is the conditional mutual information. The `max(., 0)` clamp handles floating-point errors. **Correct**.

---

### Consequence Decomposition (Theorem 4c)

#### ⚪ F-MATH-07: Three-way decomposition is correctly computed

`E = I(X;Y) + H(M|Y) + H(X|M)` where:
- `I(X;Y) = H(X) + H(Y) - H(X,Y)` ← consequence
- `H(M|Y) = H(M,Y) - H(Y)` ← hedge
- `H(X|M) = H(X,M) - H(M)` ← latitude

All computed with compensated summation. **Correct**.

---

### Depth Ledger (Proposition 6)

#### 🟡 F-MATH-08: `computeDepthLedger` uses form as W₀ and levels as W₁..W_L

**File**: [`Stats.cpp`](../src/Stats.cpp#L386-L455)

The telescopic identity `H(X) = H(W_L) + Σ H(W_{k-1}|W_k)` is computed by building joint distributions at each level and computing conditional entropies. The identity check in `DepthLedger::satisfiesIdentity()` validates this. **Correct**.

One subtlety: level indices that exceed `step.levels.size()` default to `0ULL`, which collapses all forms to one cluster. This is correct behavior for steps with fewer hierarchy levels.

---

## Channel.cpp — Steganographic Embedding

### Capacity Analysis (Theorem 8)

#### ⚪ F-CHAN-01: `analyzeCapacity` correctly computes mean and variance

```cpp
h += -p * log2p;                    // H(X|w)
secondMoment += p * (log2p * log2p); // E[(-log2 P)^2 | w]
var = max(0.0, secondMoment - h*h);  // Var = E[X^2] - E[X]^2
```

Standard one-pass variance computation. The `max(0.0, ...)` clamp handles floating-point cancellation. **Correct**.

The safe payload bound `k = floor(μ - 3σ - 7)` matches Theorem 8's construction. The `-7` provides a large safety margin beyond the 3σ Chebyshev bound.

---

### Embed/Decode (Proposition 9)

#### 🟠 F-CHAN-02: `embed()` uses `double` arithmetic — precision loss for long sequences

**File**: [`Channel.cpp`](../src/Channel.cpp#L148-L214)

The arithmetic coding loop updates `curL` and `curR` (both `double`) by narrowing intervals:
```cpp
curL = subL;
curR = subR;
```

For long sequences, `curR - curL` can become extremely small, and `double` (53-bit mantissa) loses precision. The header documents this: *"For long sequences where P > 53 bits, interval arithmetic under IEEE 754 `double` experiences precision underflow."*

However, the `embed` function does **not** check for precision underflow. It does check `payloadBits > 53` (line 154), but even for `payloadBits <= 53`, a long sequence with many options can narrow the interval below `double` precision.

**Severity**: 🟠 HIGH — silent precision loss can cause incorrect embedding choices
**Fix**: Add interval width check after each narrowing step: if `curR - curL < ε` (e.g., `1e-15`), return `success = false`.

---

#### 🟡 F-CHAN-03: `decode()` has asymmetric precision checks vs `embed()`

**File**: [`Channel.cpp`](../src/Channel.cpp#L216-L301)

`decode()` checks `if (q >= cellWidth)` (line 268) where `q = curR - curL`. This check is correct but inverted in semantics — it returns failure when the interval is **too wide** to uniquely identify a cell. But it doesn't check for **too narrow** (precision underflow), which is the more likely failure mode.

The circular modular arithmetic (lines 273-290) uses `±1e-12` tolerance for boundary wrap-around, which is reasonable for `double`.

**Severity**: 🟡 MEDIUM — missing precision underflow check
**Fix**: Add `if (curR - curL < 1e-15) { res.success = false; return res; }` after the narrowing loop.

---

#### 🟡 F-CHAN-04: `exactFailureProbability` uses quantized log-likelihoods

**File**: [`Channel.cpp`](../src/Channel.cpp#L94-L146)

```cpp
constexpr double kScale = 1000.0;
const auto shift = static_cast<std::int64_t>(std::round(logP * kScale));
```

Log-probabilities are quantized to 0.001-bit resolution. For very small probabilities (e.g., `p = 1e-10`, `logP ≈ 33.2`), the quantized bin `shift ≈ 33219` is accurate. The `kMaxDistStates = 4096` prune limit discards low-probability tails. This introduces a bounded approximation error.

**Severity**: 🟡 MEDIUM — documented as approximation, but the error bound is not computed
**Fix**: Document the approximation error bound in the header comment.

---

## Summary

| ID | Component | Severity | Description |
|:---|:----------|:---------|:------------|
| F-MATH-02 | Stats | 🟡 | `J_bound` clamping undocumented |
| F-CHAN-02 | Channel | 🟠 | No precision underflow check in `embed()` |
| F-CHAN-03 | Channel | 🟡 | Missing precision underflow check in `decode()` |
| F-CHAN-04 | Channel | 🟡 | Quantized exact failure probability without error bound |

All core information-theoretic computations (entropy, MI, JSD, leakage, ledger) are **mathematically correct** with proper compensated summation.
