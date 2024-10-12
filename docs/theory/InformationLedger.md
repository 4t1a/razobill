# The Information Ledger (Theorem 1)

`librazobill` is grounded in the information-theoretic framework introduced in [*Latitude and Doubt: An exact ledger of meaning in messages with measurements on the genetic code and on English*](https://doi.org/10.5281/zenodo.23287286) (M. M. Atta, 2026; Zenodo DOI: [10.5281/zenodo.23287286](https://doi.org/10.5281/zenodo.23287286)). This document details the mathematical identities, proofs, and numerical precision guarantees governing log compaction and semantic projection.

---

## 1. Mathematical Definitions

Let $X$ denote a discrete random variable representing the universe of raw log messages distributed according to empirical probability mass function $P(X)$.

Let $f: X \to W$ be the deterministic parsing function mapping each message $x \in X$ to its canonical structural form (template) $w \in W$.

We define the primary information-theoretic quantities:

| Quantity | Mathematical Formulation | Physical Meaning in Logging |
|---|---|---|
| **Message Entropy ($E$)** | $E = H(X) = -\sum_{x} P(x) \log_2 P(x)$ | Total information content (uncertainty) in raw log messages. |
| **Form Entropy ($C$)** | $C = H(W) = -\sum_{w} P(w) \log_2 P(w)$ | Information content residing in template structures alone. |
| **Mutual Information ($I$)** | $I = I(X; W) = H(X) + H(W) - H(X, W)$ | Shared information between message text and mined templates. |
| **Latitude ($\Lambda$)** | $\Lambda = H(X \mid W) = -\sum_{w} P(w) \sum_{x} P(x \mid w) \log_2 P(x \mid w)$ | Residual uncertainty in variable slot values given the template. |
| **Doubt ($\Delta$)** | $\Delta = H(W \mid X) = -\sum_{x} P(x) \sum_{w} P(w \mid x) \log_2 P(w \mid x)$ | Ambiguity in template choice given the raw log message. |

---

## 2. The Information Identities (Theorem 1, Atta 2026)

### Theorem 1
For any joint distribution of raw messages $X$ and representations $W$:
$$E = I + \Lambda$$
$$C = I + \Delta$$

### Proof for Deterministic Form Extraction
Because form extraction in `librazobill` is a pure deterministic function $W = f(X)$:
1. For every message $x$, there exists exactly one form $w = f(x)$. Therefore:
   $$P(w \mid x) = \begin{cases} 1 & \text{if } w = f(x) \\ 0 & \text{otherwise} \end{cases}$$
2. Substituting $P(w \mid x)$ into the conditional entropy formula:
   $$\Delta = H(W \mid X) = -\sum_{x} P(x) \left[ 1 \cdot \log_2(1) \right] \equiv 0$$
3. Because $\Delta = 0$, the identity $C = I + \Delta$ simplifies to:
   $$I(X; W) = H(W) = C$$
   *The mutual information between a log stream and its canonical forms equals the entropy of the forms themselves.*
### The Complete Balance Theorem (Theorem 4, Atta 2026)
Subtracting the two fundamental identities yields:
$$E - C = \Lambda - \Delta$$

The joint entropy of expression and form decomposes as:
$$H(W, X) = I + \Lambda + \Delta = C + \Lambda = E + \Delta$$

```
                           H(W, X) = Joint Information
    ┌──────────────────────────────────────┬─────────────────────────────────────┐
    │          What Is Said (E)            │          What Is Meant (C)          │
    │  E = H(X) = Expression Entropy      │    C = H(W) = Content Entropy       │
    ├──────────────────┬───────────────────┼───────────────────┬─────────────────┤
    │  Conveyance (I)  │   Latitude (Λ)    │  Conveyance (I)   │    Doubt (Δ)    │
    │   I = I(W; X)    │  Λ = H(X | W)     │    I = I(W; X)    │  Δ = H(W | X)   │
    │   What form      │  Speaker freedom  │    What form      │  What form      │
    │   conveys of     │  once meaning is  │    conveys of     │  fails to       │
    │   meaning        │  fixed            │    meaning        │  reveal         │
    └──────────────────┴───────────────────┴───────────────────┴─────────────────┘
```

### Corollary 2: The Vocabulary Law
For any structured logging system with $K$ variable slots where slot $k$ draws tokens from vocabulary $\mathcal{V}_k$, the total variable latitude is bounded above by:
$$\Lambda \le \sum_{k=1}^K \log_2 |\mathcal{V}_k|$$
Equality holds if and only if each slot is uniformly distributed and statistically independent of all other slots conditioned on form $W$.

### Proposition 3: Variation of Information as Metric Distance
The true information distance $D(W, X)$ between the semantic form and raw message space is given by the variation of information:
$$D(W, X) = H(W \mid X) + H(X \mid W) = \Delta + \Lambda = E + C - 2I$$
When doubt vanishes ($\Delta = 0$), $D(W, X) = \Lambda$. The distance from message to form is exactly the latitude of the slots.

```text
┌────────────────────────────────────────────────────────────────────────────────────────┐
│               VARIATION OF INFORMATION AS METRIC DISTANCE: D(W, X)                     │
└────────────────────────────────────────────────────────────────────────────────────────┘
                    ┌───────────────────────────────────────────┐
                    │            Total Joint Entropy            │
                    │               H(W, X) = E + Δ             │
                    └─────────────────────┬─────────────────────┘
                                          │
            ┌─────────────────────────────┴─────────────────────────────┐
            ▼                                                           ▼
  ┌───────────────────────────────────┐               ┌───────────────────────────────────┐
  │     Doubt: Δ = H(W | X)           │               │     Latitude: Λ = H(X | W)        │
  │     (Form ambiguity given msg)    │               │     (Slot variation given form)   │
  │     ≡ 0 in Razobill (Deterministic│               │     Remaining parametric degree   │
  │       Parsing Homomorphism)       │               │     of freedom                    │
  └─────────────────┬─────────────────┘               └─────────────────┬─────────────────┘
                    │                                                   │
                    └─────────────────────────┬─────────────────────────┘
                                              ▼
                              D(W, X) = Δ + Λ ≡ Λ (Metric Distance)
                              Satisfies: D(W, X) ≥ 0, D(W, X) = D(X, W)
                              and Triangle Inequality: D(X, Z) ≤ D(X, Y) + D(Y, Z)
```

### Proposition 6: Multi-Level Depth Ledger
Let $W_1 = f_1(X), W_2 = f_2(W_1), \dots, W_L = f_L(W_{L-1})$ be successive functional coarsenings, with $W_0 = X$.
Entropy and purpose relevance telescope across the levels:
$$H(X) = H(W_L) + \sum_{k=1}^L H(W_{k-1} \mid W_k)$$
$$I(X; Y) = I(W_L; Y) + \sum_{k=1}^L I(W_{k-1}; Y \mid W_k)$$
The term $k = 1$ is the finest level: latitude $\Lambda = H(X \mid W_1)$ and purpose leakage $L_Y(W_1) = I(X; Y \mid W_1)$.

```text
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                        PROPOSITION 6: MULTI-LEVEL DEPTH LEDGER                         │
└────────────────────────────────────────────────────────────────────────────────────────┘
  Entropy Decomposition:
  H(X) = H(W_L) + H(W_L-1 | W_L) + ... + H(W_1 | W_2) + H(X | W_1)
         └─────┘  └──────────────────────────────────┘  └────────┘
        Coarsest            Intermediate Steps          Finest Latitude (Λ)

  Relevance Decomposition:
  I(X; Y) = I(W_L; Y) + I(W_L-1; Y | W_L) + ... + I(W_1; Y | W_2) + I(X; Y | W_1)
            └───────┘   └───────────────────────────────────────┘   └───────────┘
            Coarsest             Intermediate Relevance             Finest Leakage (L_Y)
```

### Theorem 7: The Price of Saying
Let $\ell_W, \ell_X, \ell_{W \mid X}$ be the smallest expected lengths in bits of optimal prefix codes for sequences of $n$ independent messages.
$$nC \le \ell_W < nC + 1$$
$$nE \le \ell_X < nE + 1$$
$$n\Delta \le \ell_{W \mid X} < n\Delta + 1$$

Sending form $X^n$ and then resolving meaning costs between $n(E + \Delta)$ and $n(E + \Delta) + 2$ bits:
$$n(E + \Delta) = nC + n\Lambda$$
Saying instead of meaning costs exactly $n\Lambda$ bits per sequence (the premium of wording), whatever the doubt $\Delta$.

### Theorem 8 & Proposition 9: The Latitude Channel & Finite Precision
In an unambiguous source ($\Delta = 0$), latitude $\Lambda = H(X \mid W)$ is an operational transmission channel.
For a meaning sequence $w^n$:
1. **Mean Capacity & Variance**:
   $$\mu = \sum_{i=1}^n H(X \mid w_i), \quad \sigma^2 = \sum_{i=1}^n \mathrm{Var}[-\log_2 Q(X \mid w_i)]$$
2. **Safe Payload Rate**:
   $$k = \lfloor \mu - 3\sigma - 7 \rfloor$$
3. **Exact Failure Probability**:
   $$\delta = \sum_{x^n} Q(x^n \mid w^n) \min\{1, 2^k Q(x^n \mid w^n)\}$$
   $$\delta \le \frac{\sigma^2}{(\mu - k - s)^2} + 2^{-s}$$
4. **Proposition 9 Finite Precision**:
   On dyadic grid $G_P = \{j 2^{-P} : 0 \le j < 2^P\}$, key bits requirement:
   $$P \ge \left\lceil \sum_{i=1}^n \max_x (-\log_2 Q(x \mid w_i)) \right\rceil + s + 1$$
   guarantees total variation distance to cover law $\le 2^{-s-1}$ and failure probability within $2^{-s} + 2^{k+1-P}$ of $\delta$.

---

In `librazobill`, lossless compaction is mathematically guaranteed because:
$$X \equiv (W, \text{ints}, \text{strs})$$
Every raw message $x$ can be recovered with 100% byte-for-byte fidelity via `reconstruct(form, ints, strs)`.

In contrast, heuristic log clusterers (e.g. Drain3) fail this condition:
- Drain replaces variables with wildcards `<*>` and discards variable values.
- Reconstructing $x$ from Drain's template is impossible, resulting in non-zero information destruction.

---

## 4. Neumaier Compensated Floating-Point Summation

Calculating entropy sums across millions of events causes floating-point roundoff accumulation in standard IEEE-754 arithmetic.

[`include/librazobill/Stats.hpp`](../../include/librazobill/Stats.hpp) eliminates roundoff drift using Neumaier compensated summation:

```cpp
struct NeumaierSum {
    double sum = 0.0;
    double comp = 0.0; // Compensation accumulator

    void add(double value) noexcept {
        double t = sum + value;
        if (std::abs(sum) >= std::abs(value)) {
            comp += (sum - t) + value;
        } else {
            comp += (value - t) + sum;
        }
        sum = t;
    }

    [[nodiscard]] double total() const noexcept {
        return sum + comp;
    }
};
```

This guarantees that across hundreds of millions of events, the ledger identities ($|E - (I + \Lambda)| < 10^{-9}$ and $|C - (I + \Delta)| < 10^{-9}$) hold to double-precision tolerance.
 
---
 
## 5. Academic Citation & Foundations
 
For the foundational mathematical proofs and empirical validations, cite:
 
> **Atta, Moustafa Mahmoud (2026).** *Latitude and Doubt: An exact ledger of meaning in messages with measurements on the genetic code and on English*.  
> Zenodo. DOI: [10.5281/zenodo.23287286](https://doi.org/10.5281/zenodo.23287286) | Record: [zenodo.org/records/23287286](https://zenodo.org/records/23287286)
 
```bibtex
@article{atta2026latitude,
  author    = {Atta, Moustafa Mahmoud},
  title     = {Latitude and Doubt: An exact ledger of meaning in messages with measurements on the genetic code and on English},
  year      = {2026},
  journal   = {Zenodo Preprint},
  doi       = {10.5281/zenodo.23287286},
  url       = {https://doi.org/10.5281/zenodo.23287286}
}
```

