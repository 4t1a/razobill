# Architecture & Identity Decisions: Razobill v1.1.1

## 1. Documentation Identity Selection

### Essence Statement
Razobill is an exact, invertible telemetry engine that decomposes log strings into structured templates, numbers, and words, turning unstructured text into queryable columnar data with zero byte loss.

### Evaluated Directions
1. **The Systems Mechanic**: Terse, memory-arena-centric, zero-allocation C/SIMD tool written exclusively for systems engineers optimizing cache lines and registers.
2. **The Precision Log Inverter**: Grounded, mathematically rigorous, and mechanically exact; treats log parsing as an invertible algebraic decomposition rather than probabilistic clustering, speaking directly to data engineers and database practitioners.
3. **The Observability Minimalist**: A fast, plug-and-play CLI and Python utility that claims only to be a drop-in replacement for regex and Drain.

### Selected Direction: Direction 2 — *The Precision Log Inverter*
**Rationale**:
- Razobill's core differentiator across the industry is not just raw SIMD scanning speed, nor is it a trivial script. Its genuine value proposition is **exact invertibility**: any log line decomposed into a template and its parameter slots can be restored byte-for-byte on demand (`join(*split(x)) == x`).
- Direction 2 preserves the intellectual integrity of the project while cutting out academic hyperbole, ungrounded paper jargon, and marketing hype.
- It unifies all three consumer tiers: C/C++ systems developers, DuckDB SQL data analysts, and Python telemetry engineers.

---

## 2. Voice & Tone Framework

### Trait 1: Definite and Factual
- We state exact technical realities, data structures, and algorithmic invariants without hedging or hyperbole.
- **Never sounds like**: Academic throat-clearing, marketing buzzwords ("revolutionary", "blazing-fast", "game-changing"), or hand-waving claims.

### Trait 2: Mechanically Exact
- Every code sample is runnable and demonstrates exact byte invertibility. Numbers, flags, parameters, and return types match the source code verbatim.
- **Never sounds like**: Pseudo-code, approximate outputs, or hypothetical benchmarks without hardware and corpus context.

### Trait 3: Unadorned and Respectful of the Reader's Time
- One idea per paragraph. Short sentences. Concrete examples first, underlying engine mechanics second.
- **Never sounds like**: Chatty conversational filler ("Let's dive right in!", "In today's fast-paced cloud landscape"), defensive rants against other tools, or whimsical emojis.

---

## 3. Surface & Symbol Invariants

### 3.1 DuckDB Extension
- Surface budget: Exactly 4 scalar functions, 0 aliases.
- Functions: `RzSplit(line)`, `RzJoin(template, ints, strs)` (also varargs), `RzTemplate(line)`, `RzHash(line)`.
- Legacy `rb*` functions are completely removed.

### 3.2 Python Library (`pyrazobill`)
- Canonical functions: `RzSplit`, `RzJoin`, `RzTemplate`, `RzSplitMany`.
- Ergonomic aliases: `split`, `join`, `template`, `split_many`.
- Containers: `RzRecord` (named tuple `(template, ints, strs)`).
- Error class: `RzError`.
- Version: `__version__ = "1.1.1"`.

### 3.3 Universal C ABI (`razobill.h`)
- Functions: `RzSplit`, `RzJoin`, `RzTemplate`, `RzFree`, `RzStatusMessage`, `RzVersion`.
- Structs: `RzRecord`, `RzStatus`.

### 3.4 Template Syntax
- Integer placeholder: `{i}`.
- String placeholder: `{s}`.
- Escaped literal braces: `{{` and `}}`.
