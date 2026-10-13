# Related Work and Foundations

This document establishes the prior art and theoretical foundation of Razobill, summarizing what each source establishes, what Razobill adapts or takes from it, and how Razobill differs.

---

## 1. Domain-Specific Log Compression & Systems
1. **CLP (Rodrigues, Luo, Yuan; USENIX OSDI 2021)**:
   - *Establishes*: Separation of static log text schemas from numerical variables into columnar files with dictionary encoding, enabling sub-second search over compressed logs without full decompression.
   - *What Razobill takes*: Decomposition of log streams into structural skeletons and variable argument slots.
   - *How Razobill differs*: Razobill provides an exact, deterministic homomorphism ($W = f(X), \Delta = 0$) embedded natively as an in-database extension (DuckDB C_STRUCT ABI and ClickHouse MergeTree) with zero-copy SIMD tokenization rather than a standalone file format and search engine.

2. **LogReducer (Wei et al.; USENIX FAST 2021)**:
   - *Establishes*: Parser-based compression for petabyte-scale cloud logs with specialized numerical variable compression (delta timestamps, correlation identification, elastic integer encoding).
   - *What Razobill takes*: Recognition that numerical variables dominate log payload size and require native integer array representations (`ints` vector) rather than string tokens.
   - *How Razobill differs*: LogReducer is an offline batch compression pipeline; Razobill operates as an online single-pass analytical engine with real-time semantic gating ($\Delta L \le \varepsilon$).

3. **LogShrink (Li, Zhang, Le, Chen; IEEE/ACM ICSE 2024)**:
   - *Establishes*: Structure-aware log compression using clustering-based sequence sampling and commonality/variability analysis via Longest Common Subsequence (LCS).
   - *What Razobill takes*: Principled separation of invariant common template structures from variable parameters.
   - *How Razobill differs*: LogShrink relies on multi-pass LCS and clustering heuristics that scale quadratically with token length; Razobill uses a linear $O(N)$ single-pass byte-classifier table (`detail::kClass`) with zero runtime heap allocation.

4. **DeLog (Lyu et al.; arXiv:2601.15084, 2026)**:
   - *Establishes*: First empirical proof that higher log parsing accuracy does *not* necessarily imply a higher compression ratio, showing that compression is dictated by effective low-entropy pattern-based grouping.
   - *What Razobill takes*: Mathematical validation that grouping into structural form keys ($W$) is an entropy minimization problem over variable slot distributions rather than a string clustering benchmark.
   - *How Razobill differs*: DeLog performs pattern signature synthesis across token positions; Razobill evaluates grouping adequacy via the information gate ($\Delta I = \pi_{A \cup B} \operatorname{JSD}(P_A, P_B)$) grounded in *Latitude and Doubt*.

5. **Loghub-2.0 (Jiang et al.; ACM ISSTA 2024, arXiv:2308.10828)**:
   - *Establishes*: A rigorous benchmark suite of 14 datasets averaging 3.6 million log lines per system, standardizing evaluation across 15 log parsers and revealing that previous small benchmarks masked parser failure on rare events.
   - *What Razobill takes*: Standardized evaluation methodology and benchmark corpora for empirical telemetry auditing.
   - *How Razobill differs*: Loghub benchmarks probabilistic parsers against human-annotated regex templates; Razobill evaluates mathematical invertibility ($m' \equiv m$) and information conservation.

6. **Drain (He, Zhu, Zheng, Lyu; IEEE ICWS 2017)**:
   - *Establishes*: Fixed-depth parse tree heuristic for fast online streaming log parsing.
   - *What Razobill takes*: The benchmark baseline against which legacy observability log parsing is compared.
   - *How Razobill differs*: Drain is lossy, probabilistic, discards variable identities into wildcards (`<*>`), and leaks information ($H(W \mid X) > 0$); Razobill is an exact algebraic projection ($H(W \mid X) \equiv 0$) guaranteeing 100% byte-for-byte lossless reconstruction.

---

## 2. Low-Level Vectorization & Database Text Encodings
7. **simdjson (Langdale and Lemire; VLDBJ 2019)**:
   - *Establishes*: Multi-stage SIMD vectorization parsing gigabytes of structured text per second using 64-byte vector loads, structural bitmasks, and carry-less operations.
   - *What Razobill takes*: 32-byte (AVX2) and 64-byte (AVX-512) vector scanning with trailing-zero bitmask index location (`tzcnt` / `ctz`).
   - *How Razobill differs*: simdjson constructs a structural tape over JSON grammar; Razobill executes fused delimiter classification and slot extraction over arbitrary log text.

8. **FSST (Boncz, Neumann, Leis; PVLDB 2020)**:
   - *Establishes*: Fast Static Symbol Table string compression enabling random-access substring decompression and compressed evaluation directly in database engines.
   - *What Razobill takes*: The principle that database text columns should remain compressed in memory and decompressed on demand.
   - *How Razobill differs*: FSST compresses arbitrary string byte sequences via an 8-byte symbol table; Razobill decomposes structured strings into typed columnar slot tables (`logs_c`: form ID + integer vectors + dictionary string pools).

---

## 3. Information Theory & Finite-Sample Entropy Estimation
9. **Miller-Madow (Miller; Info. Theory in Psychology 1955)**:
   - *Establishes*: First-order asymptotic bias correction for plug-in entropy estimators: $\hat{H}_{\text{MM}}(X) = \hat{H}(X) + \frac{K_{\text{obs}} - 1}{2N \ln 2}$.
   - *What Razobill takes*: Direct implementation in [`Stats.cpp`](../src/Stats.cpp) (`millerMadow()`) and DuckDB UDF (`rb_miller_madow`) for finite-sample entropy correction.
   - *How Razobill differs*: Razobill stratifies the Miller-Madow correction across conditional entropy slices in the information ledger ($H_{\text{MM}}(X \mid W)$ and $H_{\text{MM}}(W \mid X)$).

10. **Chao-Shen (Chao and Shen; Env. & Ecol. Stat. 2003)**:
    - *Establishes*: Nonparametric Good-Turing sample coverage estimator $\hat{C} = 1 - f_1 / N$ applied to Horvitz-Thompson entropy estimation to correct for unobserved categories.
    - *What Razobill takes*: Reference point for comparative finite-sample bias evaluation in undersampled log streams.
    - *How Razobill differs*: Razobill relies on Miller-Madow correction and Proposition 10 certified intervals to bound coarse meaning alphabets.

11. **NSB (Nemenman, Shafee, Bialek; NeurIPS 2001)**:
    - *Establishes*: Bayesian entropy estimation using a continuous mixture of Dirichlet priors designed to maintain a flat prior over entropy $H$ in severely undersampled regimes ($N \ll K$).
    - *What Razobill takes*: Theoretical understanding of the variance and bias boundaries when $N < K$.
    - *How Razobill differs*: Razobill avoids $O(K)$ computational overhead of hyperparameter integration, utilizing closed-form $O(1)$ estimators for real-time per-chunk analytical query processing.

12. **Paninski (Paninski; Neural Computation 2003)**:
    - *Establishes*: Theoretical minimax bounds for entropy estimation, proving that consistent estimation of entropy requires $N \to \infty$ and $N / \ln K \to \infty$.
    - *What Razobill takes*: Theoretical foundation for acknowledging the undersampled regime and requiring explicit bounds on confidence intervals.
    - *How Razobill differs*: Razobill pairs empirical estimates with distribution-free Bretagnolle/Huber-Carol/Weissman bounds (Proposition 10) for small diagnostic purpose alphabets.

---

## 4. Grounding Preprint: Atta (2026)
13. **Atta, M. M. (2026), *Latitude and Doubt: An exact ledger of meaning in messages with measurements on the genetic code and on English***, Zenodo DOI: [10.5281/zenodo.23287286](https://doi.org/10.5281/zenodo.23287286):
    - *Establishes*: The information ledger ($E = I + \Lambda, C = I + \Delta, d(W, X) = \Lambda + \Delta$), deterministic invertibility ($\Delta = 0$), semantic gating ($\Delta L \le \varepsilon$), Price of Saying bounds ($n(E + \Delta) = nC + n\Lambda$), and the latitude channel.
    - *What Razobill takes*: The exact mathematical accounting framework implemented in [`Stats.hpp`](../include/librazobill/Stats.hpp), [`Stats.cpp`](../src/Stats.cpp), and [`Channel.cpp`](../src/Channel.cpp).
    - *How Razobill differs*: Atta (2026) is a theoretical information-theoretic paper evaluating genetic codons and English linguistics; Razobill is a C++20 database engine and DuckDB extension operationalizing these theorems for production computing telemetry.
