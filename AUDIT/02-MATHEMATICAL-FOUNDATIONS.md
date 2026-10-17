# Mathematical Foundations & Information-Theoretic Accounting

**Audit Subject**: Rigorous Mathematical Derivations, Conservation Laws, and Numerical Precision  
**Foundational Literature**: *"Latitude and Doubt: An exact ledger of meaning in messages with measurements on the genetic code and on English"* (M. M. Atta, Zenodo, 2026)  

---

## 1. The Information-Theoretic Ledger (Theorem 1)

In classical Shannon information theory, messages are treated as physical signals $X$ emitted across a channel. In semantic systems, every observed message $X$ is associated with an intended semantic content (meaning) $W$.

Atta (2026) establishes that semantic communication is governed by five exact conserved quantities:

$$\begin{aligned}
E &= H(X) && \text{Expression: What is said (entropy of surface forms)} \\
C &= H(W) && \text{Content: What is meant (entropy of semantic intentions)} \\
\Lambda &= H(X \mid W) && \text{Latitude: Speaker's freedom in wording (synonymy / redundancy)} \\
\Delta &= H(W \mid X) && \text{Doubt: Receiver's uncertainty regarding meaning (polysemy / ambiguity)} \\
I &= I(X; W) && \text{Conveyance: Information successfully transmitted about meaning}
\end{aligned}$$

### 1.1 The Fundamental Conservation Laws
From the Kolmogorov-Shannon chain rule of conditional entropy:

$$E = I + \Lambda \quad \text{and} \quad C = I + \Delta$$

Subtracting these two identities yields the **Fundamental Balance Law**:

$$E - C = \Lambda - \Delta$$

- If a language has more ways of expressing thoughts than thoughts themselves ($E > C$), it must possess surplus Latitude ($\Lambda > \Delta$).
- In an unambiguous code ($\Delta = 0$), Content equals Conveyance ($C = I$), and Expression decomposes strictly into $C + \Lambda$.
- In a code with zero redundancy ($\Lambda = 0$), Expression equals Conveyance ($E = I$).

### 1.2 Proposition 3: Semantic Metric Distance
The total distance between form space $X$ and meaning space $W$ forms a pseudometric:

$$d(W, X) = \Lambda + \Delta = H(W, X) - I(W; X)$$

This distance equals zero if and only if there is a deterministic bijective isomorphism between messages and meanings.

---

## 2. Numerical Stability & The Fréchet Polytope Projection

Empirical estimation of joint distributions from finite samples often introduces floating-point errors and sampling bias. If joint entropy $J = H(X, W)$ is computed naively from empirical cell counts, numerical noise can cause:

$$J < \max(H(W), H(X)) \implies \Lambda < 0 \text{ or } \Delta < 0$$

which violates fundamental non-negativity laws of conditional entropy.

### 2.1 Fréchet Polytope Projection in `Stats.cpp`
To enforce absolute mathematical consistency, `Stats.cpp` projects empirical joint entropy $J$ into the Fréchet entropy polytope:

$$\max(C, E) \le J_{\text{bound}} \le C + E$$

Implemented in `src/Stats.cpp`:
```cpp
const double J_bound = std::max({C, E, std::min(J, C + E)});
const double Lambda  = J_bound - C;
const double Delta   = J_bound - E;
const double I       = C + E - J_bound;
```
**Audit Assessment**:
- Guarantees $\Lambda \ge 0$, $\Delta \ge 0$, and $I \ge 0$ unconditionally.
- Preserves the algebraic identities $I + \Lambda = E$, $I + \Delta = C$, and $E - C = \Lambda - \Delta$ within machine epsilon ($< 10^{-15}$).

---

## 3. High-Precision Arithmetic: Neumaier Compensated Summation

When computing Shannon entropy $\sum -p_i \log_2(p_i)$ across millions of rare log templates, standard IEEE 754 floating-point addition accumulates $O(N \cdot \epsilon)$ rounding error and suffers from catastrophic cancellation.

`Stats.cpp` implements Arnold Neumaier's compensated summation algorithm (Neumaier, 1974):

```cpp
class Sum {
public:
    void add(double x) noexcept {
        const double t = s_ + x;
        c_ += std::abs(s_) >= std::abs(x) ? (s_ - t) + x : (x - t) + s_;
        s_ = t;
    }
    [[nodiscard]] double value() const noexcept { return s_ + c_; }
private:
    double s_ = 0.0, c_ = 0.0;
};
```
- **Error Bound**: Maintains cumulative rounding error at $O(\epsilon)$ instead of $O(N \cdot \epsilon)$, eliminating precision drift across arbitrarily large log streams.

---

## 4. The Closed-Form Information Gate (Theorem 3 & Theorem 5)

When grouping log forms into equivalence classes for compaction, downstream operational purpose $Y$ (e.g., error alert status) must not be corrupted.

### 4.1 Theorem 3: Gate Closed Form via Jensen-Shannon Divergence
Merging two forms $A$ and $B$ incurs an exact mutual information loss given by the volume-weighted Jensen-Shannon Divergence of their posterior distributions:

$$\Delta I = I(X; Y) - I(W; Y) = \pi_{A \cup B} \cdot \operatorname{JSD}(P_A, P_B)$$

Where:
$$\operatorname{JSD}(P_A, P_B) = H\left(\frac{\pi_A P_A + \pi_B P_B}{\pi_{A \cup B}}\right) - \frac{\pi_A H(P_A) + \pi_B H(P_B)}{\pi_{A \cup B}}$$

Implemented in `src/Stats.cpp` (`mergeCost`):
```cpp
Sum jsd;
for (const auto& [key, n] : nfy) {
    const double p_yf = n / nf[key.first];
    const double p_yF = ny[key.second] / N;
    if (p_yf > 0.0 && p_yF > 0.0) {
        jsd.add((n / N) * std::log2(p_yf / p_yF));
    }
}
return {.jsd = std::max(jsd.value(), 0.0), .share = totalEvents > 0.0 ? N / totalEvents : 0.0};
```

### 4.2 Theorem 5: Semantic Leakage
Leakage measures the information about purpose $Y$ destroyed by abstracting forms $X$ into meanings $W$:

$$L_Y(W) = I(X; Y \mid W) = H(X, W) + H(Y, W) - H(W) - H(X, Y, W)$$

- $L_Y(W) = 0$ proves that $W$ is a **sufficient statistic** for purpose $Y$. The Gate refuses any merge where leakage exceeds threshold $\epsilon$.

---

## 5. Consequence Decomposition & The Price of Saying (Theorems 4c & 7)

### 5.1 Theorem 4c: Three-Way Decomposition
Total emission entropy $H(X)$ decomposes into operational relevance, consequence hedge, and wording latitude:

$$E = I(X; Y) + H(M \mid Y) + H(X \mid M)$$

1. $I(X; Y)$ (**Consequence**): True information about purpose $Y$.
2. $H(M \mid Y)$ (**Hedge**): Receiver's internal confidence variations unexplained by truth.
3. $H(X \mid M)$ (**Latitude**): Variations in surface form to which purpose $Y$ is completely blind.

### 5.2 Theorem 7: The Price of Saying
Communication cost over $n$ events:

$$n(E + \Delta) = nC + n\Lambda$$

Saying instead of meaning incurs an unavoidable wording tax of exactly $n \cdot \Lambda$ bits.

---

## 6. Latitude Steganographic Channel (Theorem 8 & Proposition 9)

In an unambiguous semantic source ($\Delta = 0$), Latitude $\Lambda = H(X \mid W)$ represents operational transmission capacity: watermark or provenance bits can be embedded into the choice of synonymous templates without altering meaning.

### 6.1 Capacity and Safe Payload
For a sequence of independent synonymous choices:

$$\mu = \sum_{i=1}^n H(X_i \mid w_i), \quad \sigma^2 = \sum_{i=1}^n \operatorname{Var}(-\log_2 Q(X_i \mid w_i))$$

Safe payload bits:
$$k = \lfloor \mu - 3\sigma - 7 \rfloor$$

### 6.2 Analytical Failure Probability via Dynamic Programming
`Channel.cpp` evaluates the exact multi-convolution error probability $\delta = \sum Q \min\{1, 2^k Q\}$ using dynamic programming with a $10^{-3}$ bit quantization grid (`kScale = 1000.0`).

### 6.3 Proposition 9: Precision Underflow Risk Analysis
Proposition 9 specifies the required grid precision:

$$P = \left\lceil \sum_{i=1}^n \max_{x \in \mathcal{X}(w_i)} (-\log_2 Q(x \mid w_i)) \right\rceil + s + 1$$

**Critical Audit Finding (F-MATH-01)**:
- `embed()` and `decode()` in `src/Channel.cpp` use standard IEEE 754 64-bit floating point (`double`), which provides only 53 bits of mantissa.
- For sequences where cumulative max negative log-probability exceeds ~45 bits ($n \gtrsim 30$ choices), interval width $q = \operatorname{curR} - \operatorname{curL}$ shrinks below $10^{-15}$ and suffers floating-point underflow.
- While the implementation safely catches this (`if (curWidth < 1e-15) return false;`), long sequence embedding requires multi-precision big-integer fixed-point arithmetic (`uint128_t` or multi-limb grid) to scale indefinitely.
