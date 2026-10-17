# Glossary

[Docs](../README.md) / [Reference](glossary.md) / Glossary

This document defines canonical terms used across the Razobill project.
Use these terms consistently in documentation, code comments, and issue discussions.

---

## Canonical terms

### Template

The constant text skeleton of a log message with typed placeholders.
Placeholders indicate variable slot positions:

- `{i}`: Placeholder for an extracted 64-bit non-negative integer.
- `{s}`: Placeholder for an extracted variable string.
- `{{` and `}}`: Escaped representation of literal curly brace characters.

### Ints

An ordered array of 64-bit non-negative integers (`int64_t` in C/C++, `BIGINT` in DuckDB, `int` in Python) extracted from numeric tokens during log splitting.

### Strs

An ordered array of variable string tokens (such as IP addresses, file paths, hostnames, and identifiers) extracted during log splitting. In C/C++, string slices borrow directly from the caller's buffer as zero-copy views.

### Split

The decomposition operation (`RzSplit` or `split`) that parses an unstructured log line into a template, integer array, and string array.

### Join

The reconstruction operation (`RzJoin` or `join`) that takes a template, integer array, and string array and restores the exact original log message byte for byte.

### Round trip

The complete cycle of splitting a log line and immediately joining the resulting components:

```python
assert rz.join(*rz.split(line)) == line
```

A successful round trip produces zero byte mismatches against the raw input text.

### Semantic scanning

The default tokenizer mode (`ScanMode::Semantic`) that groups multi-modal tokens (network IP addresses, file paths, usernames, timestamps) into string placeholders (`{s}`) and standalone integers into integer placeholders (`{i}`).

### Exact scanning

The strict tokenizer mode (`ScanMode::Exact` or `DigitOnly`) that isolates purely numeric digit sequences into `{i}`, preserving non-numeric delimiter characters verbatim in the template skeleton.

### Lossless invertibility

The algebraic property guaranteeing that for every valid input sequence $x$, the reconstruction equals the input byte for byte: $\text{join}(\text{split}(x)) = x$.

### Slot

A single parameter hole inside a template (`{i}` or `{s}`) that receives an integer or string during reconstruction.

---

## Deprecated terms and replacements

| Deprecated term | Canonical replacement | Context |
| :--- | :--- | :--- |
| `rb-Decompose` | `RzSplit` / `split` | Legacy decomposition function |
| `rb-Reconstruct` / `rb-Rebuild` | `RzJoin` / `join` | Legacy reconstruction function |
| `rb-Form` | `RzTemplate` / `template` | Legacy template extractor |
| `rb-Ints` | `record.ints` | Legacy integer array accessor |
| `rb-Strs` | `record.strs` | Legacy string array accessor |
| `Form key` | `Template` | Format string with `{i}` and `{s}` placeholders |
| `Delimiter token` | `Placeholder` | `{i}` or `{s}` marker inside a template |
