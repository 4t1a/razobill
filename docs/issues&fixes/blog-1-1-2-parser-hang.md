# Dissecting a zero-allocation parser hang: escaped quotes and loop invariants

A postmortem of the quote-backslash infinite loop resolved in Razobill v1.1.2.
Read this article if you are implementing streaming tokenizers, debugging lexer loops, or auditing Razobill's parser mechanics.

---

## The symptom

In early October 2026, an edge-case test string containing Windows file paths inside quotes caused `RzSplit` to pin CPU utilization at 100%:

```python
import pyrazobill as rz

# This call hung indefinitely prior to v1.1.2:
rz.RzSplit('opened "C:\\Users\\bob\\" ok')
```

The issue also triggered on minimal boundary strings such as `"\"` and `'\'`.

Both the native C++ SIMD scanner and the pure-Python fallback (`_fallback.py`) are designed with a zero-allocation hot path and guaranteed linear $O(N)$ execution time. An infinite loop in a parser is a violation of the core invariant.

---

## Root cause analysis

In Razobill's `semantic` scanning mode, the tokenizer treats single and double quotes as delimiters for structured string values.

When a quote character is encountered at index `p`, the scanner enters an inner scan loop to locate the matching closing quote:

```python
# Lexer state before v1.1.2:
if mode == "semantic" and msg[p] in ('"', "'"):
    q = msg[p]
    if (q == '"' and p < unclosed_dquote_end) or (q == "'" and p < unclosed_squote_end):
        pass  # <--- INVARIANT VIOLATION: p was never incremented
    else:
        q_start = p
        p += 1
        escaped = False
        while p < n and (msg[p] != q or escaped):
            escaped = (msg[p] == '\\' and not escaped)
            p += 1
        if p >= n:
            # Reached end-of-buffer without finding an unescaped closing quote
            unclosed_dquote_end = p
            p = q_start + 1
            # <--- Bug 2: Fell through into token classification with stale state
```

### The failure sequence

1. **First encounter**: The scanner hit `"` at `q_start = 7` (the start of `"C:\Users\bob\" ok`).
2. **Escaped backslash**: As `p` traversed `C:\Users\bob\`, the backslash before the quote `\"` marked `escaped = True`.
3. **Delimiter misinterpretation**: Because `escaped == True`, the quote at index 21 was treated as an escaped quote rather than a closing delimiter.
4. **End of buffer**: The inner loop ran off the end of the line (`p >= n`) searching for another closing quote.
5. **Backtrack**: The parser recorded `unclosed_dquote_end = 25` and reset the pointer to `p = q_start + 1 = 8` to fall back to treating `"` as a literal character.
6. **The loop stall**: On subsequent iterations, when `p` reached any quote character while `p < unclosed_dquote_end`, the conditional branch executed `pass`. Crucially, **`p` was not incremented**. The outer `while p < n:` loop repeated infinitely with `p` unchanged.

---

## The fix

Every branch of a streaming tokenizer must maintain a strict loop invariant:

$$\forall \text{ iteration}, \quad p_{\text{next}} > p_{\text{current}}$$

In `lib/python/pyrazobill/_fallback.py`, the fallback branch now increments `p` and yields control immediately:

```python
# Fixed lexer in v1.1.2:
if mode == "semantic" and msg[p] in ('"', "'"):
    q = msg[p]
    if (q == '"' and p < unclosed_dquote_end) or (q == "'" and p < unclosed_squote_end):
        p += 1
        continue
    else:
        q_start = p
        p += 1
        escaped = False
        while p < n and (msg[p] != q or escaped):
            escaped = (msg[p] == '\\' and not escaped)
            p += 1
        if p >= n:
            if q == '"':
                unclosed_dquote_end = p
            else:
                unclosed_squote_end = p
            p = q_start + 1
            continue
```

The C++ core scanner in `include/librazobill/Scanner.hpp` applies the identical invariant, asserting non-zero forward progress before advancing to delimiter SIMD chunks.

---

## Regression test suite

To ensure permanent prevention across both implementations, we added test cases covering escape-quote boundaries, Windows file paths, and unclosed trailing quotes:

```cpp
TEST_CASE("quote-backslash edge cases and trailing backslashes") {
    const std::vector<std::string> cases = {
        "\"\\\"",
        "'\\'",
        "\"\\\"\"",
        "'\\''",
        "\"\\\\\"",
        "\"\\\\\\\"",
        "opened \"C:\\Users\\bob\\\" ok",
        "opened 'C:\\Users\\bob\\' ok",
        "foo \"bar\\\" baz",
        "\"\"\"",
        "'''",
        "\"\\\\\\\"\"",
    };
    for (const auto& s : cases) {
        CHECK(roundtrip(s, nullptr, ScanMode::Semantic) == s);
    }
}
```

The suite runs in CI across all platforms on every pull request.

---

## Availability

The fix is shipped in:

- **`pyrazobill` 1.1.2** on PyPI.
- **DuckDB community extension** (tracking PR #3087 with `ref: v1.1.2`).
- **`librazobill` 1.1.2** release binaries on GitHub.
