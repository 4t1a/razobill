# Semantic Gating & Information Leakage

When operators query log streams, they frequently group multiple distinct templates into high-level semantic categories (e.g. grouping dozens of SSH connection messages into `AUTH_FAILURE`). However, abstraction inherently discards nuance. `librazobill` introduces an **information-theoretic gate** that mathematically certifies whether a semantic grouping is safe or destructive, grounded in [*Latitude and Doubt*](https://doi.org/10.5281/zenodo.23287286) (M. M. Atta, 2026; Zenodo DOI: [10.5281/zenodo.23287286](https://doi.org/10.5281/zenodo.23287286)).

---

## 1. The Diagnostic Purpose ($Y$)

No abstraction is universally valid for every analytical task (Theorem 5d, Atta 2026, [DOI: 10.5281/zenodo.23287286](https://doi.org/10.5281/zenodo.23287286)). An abstraction that is safe for measuring system uptime may destroy critical information needed for security intrusion detection.

Therefore, `librazobill` requires declaring a **diagnostic purpose column** $Y$ (e.g., `severity_level`, `http_status`, `error_code`, `component`).

---

## 2. Information Leakage ($L_Y$)

Let $X$ denote raw messages, $W$ denote canonical forms, and $M = g(W)$ denote a proposed semantic grouping that maps a subset of forms $F = \{f_1, f_2, \dots, f_k\} \subseteq W$ into a single label $m$.

### Definition: Information Leakage
The information leakage $L_Y(M)$ of representation $M$ with respect to purpose $Y$ is defined as the conditional mutual information:
$$L_Y(M) = I(X; Y \mid M) = I(X; Y) - I(M; Y)$$

By the data processing inequality, $I(M; Y) \le I(X; Y)$. The difference $L_Y(M)$ measures the exact number of bits of diagnostic certainty discarded by the grouping.

---

## 3. The Jensen-Shannon Divergence Identity

### Theorem 5b (Atta 2026, [DOI: 10.5281/zenodo.23287286](https://doi.org/10.5281/zenodo.23287286))
For a deterministic merge $M$ that merges the set of forms $F$ into a single class, the incremental information leakage $\Delta L(F)$ is given by:

$$\Delta L(F) = \frac{N_F}{N} \cdot \text{JS}_\pi\left(\{P(Y \mid f) : f \in F\}\right)$$

where:
- $N$ is the total number of events in the log table.
- $N_f$ is the count of events matching form $f$.
- $N_F = \sum_{f \in F} N_f$ is the total count of events across the merged forms.
- $\pi_f = \frac{N_f}{N_F}$ is the relative prior weight of form $f$ within the merge group.
- $\text{JS}_\pi$ is the generalized, prior-weighted Jensen-Shannon divergence:
  $$\text{JS}_\pi\left(\{P(Y \mid f) : f \in F\}\right) = H\left(\sum_{f \in F} \pi_f P(Y \mid f)\right) - \sum_{f \in F} \pi_f H(P(Y \mid f))$$

### Derivation
1. Expand mutual information $I(A; Y) = \sum_{a} P(a) \text{KL}(P(Y \mid a) \parallel P(Y))$ for $A = X$ and $A = M$.
2. For all forms outside the merge set $F$, the terms in $I(X; Y)$ and $I(M; Y)$ are identical and cancel out.
3. For forms inside $F$, the $X$-terms evaluate to $\sum_{f \in F} P(f) \text{KL}(P(Y \mid f) \parallel P(Y))$.
4. Factoring through the merged distribution $P(Y \mid M) = \sum_{f \in F} \pi_f P(Y \mid f)$, the difference collapses directly to the definition of weighted Jensen-Shannon divergence multiplied by the group share $N_F / N$. $\blacksquare$

---

## 4. The Gating Decision Rule

```text
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                        SEMANTIC GATING ADMISSION PIPELINE                              │
└────────────────────────────────────────────────────────────────────────────────────────┘
                    Candidate Form Merge Rule: F = {f1, f2, ..., fk}
                    Declared Diagnostic Target: Y (e.g. Failure Severity)
                                       │
                                       ▼
                    Compute Conditional Distributions: P(Y | f)
                    Calculate Weighted Prior: π_f = N_f / N_F
                                       │
                                       ▼
                    Generalized Jensen-Shannon Divergence Calculation:
                    JS_π({P(Y | f) : f ∈ F}) = H(∑ π_f P(Y | f)) - ∑ π_f H(P(Y | f))
                                       │
                                       ▼
                    Scale by Group Population Share:
                    ΔL(F) = (N_F / N) · JS_π
                                       │
                    ┌──────────────────┴──────────────────┐
                    ▼                                     ▼
             ΔL(F) ≤ ε (Threshold)                 ΔL(F) > ε (Threshold)
                    │                                     │
                    ▼                                     ▼
         [RULE ADMITTED TO CATALOG]              [RULE ADMISSION REFUSED]
         Zero Diagnostic Leakage                 Throws GateRefusedException
         Safe Columnar Rollup                    Prevents Lossy State Collapse
```

When an operator calls `engine.defineRule(name, pattern, purpose)`, `librazobill` computes $\text{JS}_\pi$ in bits per merged event:

$$\text{Admit Rule} \iff \text{JS}_\pi \le \varepsilon$$

By default, $\varepsilon = 0.05$ bits/event.

| Empirical Scenario | Measured $\text{JS}_\pi$ | Gate Decision |
|---|---|---|
| **Homogeneous SSH Password Failures** (56 forms) | $0.00000$ bits | **ADMITTED** (Zero information loss) |
| **Homogeneous Linux Auth Failures** (10 forms) | $0.00000$ bits | **ADMITTED** (Zero information loss) |
| **Cross-System Auth Merge** (SSH + Linux, purpose: `level`) | $0.99944$ bits | **REFUSED** (`GateRefusedException`) |
If $\text{JS}_\pi > \varepsilon$, the engine refuses to admit the rule into the event catalog unless the operator explicitly passes `force = true`.

---

## 5. Consequence Decomposition (Theorem 6)

The relationship between raw message prediction and structural representation prediction is formalized by the Consequence Decomposition theorem:

$$H(Y \mid X) = H(Y \mid W) - I(X; Y \mid W) = H(Y \mid W) - L_Y(W)$$

### Diagnostic Sufficiency
- When meaning is adequate with respect to $Y$ ($L_Y(W) = 0$), $H(Y \mid X) = H(Y \mid W)$.
- This establishes that the extracted structural form $W$ contains **100% of the diagnostic predictive capacity** of the raw log message $X$. Downstream anomaly detection models, alerting engines, and classifiers need only inspect $W$ — completely screening off the raw string $X$.
- When $L_Y(W) \le \varepsilon$, the loss of diagnostic predictability is strictly bounded by $\varepsilon$ bits per message.
 
---
 
## 6. Academic Citation & Foundations
 
For the theoretical proofs on semantic sufficiency and information leakage:
 
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

