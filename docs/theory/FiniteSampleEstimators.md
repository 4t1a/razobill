# Finite-Sample Estimation & Proposition 10 Bounds

In empirical data analysis, calculating entropy directly from observed counts (the naive plug-in estimator) suffers from negative systematic bias. `librazobill` incorporates analytical bias corrections and certified distribution-free confidence intervals grounded in [*Latitude and Doubt: An exact ledger of meaning in messages with measurements on the genetic code and on English*](https://doi.org/10.5281/zenodo.23287286) (M. M. Atta, 2026; Zenodo DOI: [10.5281/zenodo.23287286](https://doi.org/10.5281/zenodo.23287286)).

---

## 1. The Concave Jensen Bias Problem

The plug-in estimator for Shannon entropy calculates:
$$\hat{H}(X) = -\sum_{k=1}^{K} \hat{p}_k \log_2 \hat{p}_k, \quad \text{where } \hat{p}_k = \frac{n_k}{N}$$

Because the function $g(p) = -p \ln p$ is strictly concave, Jensen's inequality implies:
$$\mathbb{E}[\hat{H}(X)] < H(X)$$

For finite samples $N$, the empirical estimator systematically **underestimates** the true entropy. When evaluating conditional entropies $\Lambda = H(X \mid W)$ or doubt $\Delta = H(W \mid X)$, uncorrected plug-in estimates produce over-optimistic lower bounds.

---

## 2. Miller-Madow Analytical Bias Correction

```text
┌────────────────────────────────────────────────────────────────────────────────────────┐
│               FINITE-SAMPLE BIAS ESTIMATION CURVE: PLUG-IN VS MILLER-MADOW             │
└────────────────────────────────────────────────────────────────────────────────────────┘
  Estimated
  Entropy (bits)
       ▲
       │                                     True Entropy H(X)
  H*   ├┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄
       │                             ...--─── Miller-Madow H_MM(X)
       │                   ..---─────
       │           ..-─────
       │     ..-───
       │  .-─                                ─── Plug-in MLE H_hat(X) (Underestimates)
       │ /
       │/  Negative Jensen Bias Gap = (|X_obs| - 1) / (2N ln 2)
       └────────────────────────────────────────────────────────────────────────►
       0      100        500        1,000      5,000      10,000   Sample Size (N)
```

To correct finite-sample bias, `librazobill` implements the Miller-Madow asymptotic expansion:

$$\hat{H}_{\text{MM}}(X) = \hat{H}(X) + \frac{|\mathcal{X}_{\text{obs}}| - 1}{2N \ln 2}$$

where:
- $\hat{H}(X)$ is the empirical plug-in entropy in bits.
- $|\mathcal{X}_{\text{obs}}|$ is the number of distinct outcomes observed with non-zero frequency ($n_k > 0$).
- $N$ is the total sample size.
- $\ln 2$ converts the nats-based bias term to bits.

### Implementation in `Stats.hpp`
```cpp
double millerMadow(double pluginEntropyBits, std::size_t numObservedBins, std::size_t totalCount) {
    if (totalCount == 0 || numObservedBins <= 1) return pluginEntropyBits;
    const double biasBits = static_cast<double>(numObservedBins - 1) /
                            (2.0 * static_cast<double>(totalCount) * std::log(2.0));
    return pluginEntropyBits + biasBits;
}
```

### Stratified Miller-Madow Correction for Conditional Entropy
When estimating conditional latitude $\Lambda = H(X \mid W)$, naive global pooling obscures intra-form degrees of freedom. `Stats.hpp` applies a stratified correction across each observed form stratum $w \in \mathcal{W}$:

$$\hat{H}_{\text{MM}}(X \mid W) = \sum_{w \in \mathcal{W}} \frac{N_w}{N} \left[ \hat{H}(X \mid W=w) + \frac{|\mathcal{X}_w| - 1}{2 N_w \ln 2} \right]$$

where:
- $N_w$ is the count of events belonging to form $w$.
- $|\mathcal{X}_w|$ is the number of distinct message variations observed under form $w$.

This stratified adjustment accounts for uneven support sizes across frequent vs. rare templates, ensuring accurate finite-sample calibration of variable slot latitude.

---

## 3. Proposition 10: Certified Distribution-Free Confidence Intervals

In mission-critical monitoring, operators require certified error bounds on estimated mutual information $\hat{I}(X; W)$.

### Proposition 10 (Atta 2026, [DOI: 10.5281/zenodo.23287286](https://doi.org/10.5281/zenodo.23287286))
Let $\hat{I}$ denote the empirical mutual information estimated from $N$ independent observations. For any chosen significance level $\alpha \in (0, 1)$, with probability at least $1 - \alpha$:

$$|I - \hat{I}| \le \delta, \quad \text{where } \delta = \sqrt{\frac{2 \ln(2/\alpha)}{N}}$$

This yields the certified confidence interval:
$$CI_{1-\alpha}(I) = \left[ \max(0, \hat{I} - \delta), \; \hat{I} + \delta \right]$$

```text
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                   PROPOSITION 10 CERTIFIED CONFIDENCE ENVELOPE (1 - α)                 │
└────────────────────────────────────────────────────────────────────────────────────────┘
  Mutual Info
  Estimate I
       ▲
       │        Upper Certified Bound: I_hat + δ
       │        ───────╮
       │                ╰─────────╮
       │                           ╰───────────────────
  I*   ├─────────────────── Empirical I_hat ───────────
       │                           ╭───────────────────
       │                ╭─────────╯
       │        ───────╯
       │        Lower Certified Bound: max(0, I_hat - δ)
       │        Confidence Radius: δ = sqrt(2 ln(2/α) / N)
       └────────────────────────────────────────────────────────────────────────►
       0      1,000      5,000      10,000     50,000     100,000  Sample Size (N)
```

### Properties
1. **Distribution-Free**: The bound holds for arbitrary underlying log distributions without assuming Gaussianity, stationarity, or power-law behavior.
2. **Deterministic Convergence**: As sample volume $N$ grows, the interval radius $\delta$ shrinks at the rate $O(1/\sqrt{N})$.
3. **Auditability**: When running `auditParallel()` in `pyrazobill`, the engine returns the exact certified interval for the dataset, allowing automated alerts if uncertainty exceeds operational tolerances.

---

## 4. Academic Citation & Foundations

For the foundational mathematical proofs and confidence interval derivations:

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

