# Controlled glossary

Canonical terms and exact definitions for Razobill concepts and data structures.
Read this page if you are unsure which term to use when writing documentation or reading code.
Skip this page if you already know the terminology defined in the style guide.

---

## Terminology table

Use only the canonical term for each concept.
Banned synonyms are rejected during documentation linting.

| Canonical Term | Banned Synonyms | Definition |
| :--- | :--- | :--- |
| **template** | form, formKey, skeleton, pattern, cluster | The constant text skeleton containing typed `{i}` and `{s}` placeholders. |
| **numbers** | ints, integer slots, digits, numerical values | 64-bit signed integers parsed from numeric tokens in the input log line. |
| **words** | strs, string slots, dynamic tokens, residuals | Variable substrings such as IP addresses, file paths, UUIDs, or usernames. |
| **split** | parse, decompose, extract, tokenize | The operation separating a raw log line into a template, numbers, and words. |
| **join** | reconstruct, rebuild, synthesize, format | The operation restoring the exact original log line from a template and values. |
| **semantic mode** | semantic scan, grouping mode, smart mode | Default scanning mode grouping variable tokens into `{s}` placeholders. |
| **exact mode** | raw mode, strict mode, delimiter mode | Strict tokenizer mode that isolates only pure numeric tokens and literal words. |
| **invertibility** | round-trip fidelity, reversibility, reconstruction | The guarantee that `join(*split(line)) == line` byte-for-byte on all inputs. |
| **slot record** | decomposed record, parse result | Standard data container holding a template, an integer array, and a string array. |
| **string pool** | string arena, token cache | Heap-pinned string repository providing zero-copy string views during parsing. |

---

## Detailed definitions

### Template
A template is the constant character sequence of a log message with variable fields replaced by typed markers:
- `{i}`: A placeholder for a 64-bit signed integer.
- `{s}`: A placeholder for a variable word (string).
- `{{` and `}}`: Escaped literal braces.

### Numbers
The list or vector of 64-bit signed integers extracted from numeric tokens during a split operation.
Numbers are parsed using standard fast float/integer routines (`std::from_chars`) and formatted via `std::to_chars`.

### Words
The list or vector of variable substrings extracted from tokens during a split operation.
In semantic mode, tokens representing volatile entities (such as IP addresses, hash strings, and file paths) are extracted into words.

### Semantic mode versus exact mode
- **Semantic mode** (default): Uses heuristic character analysis to group complex tokens into single string slots, preventing template proliferation across high-entropy identifiers.
- **Exact mode**: Evaluates strict whitespace and punctuation delimiters without grouping non-numeric strings into single slots.

### Invertibility
The mathematical and operational invariant of Razobill:
```text
join(split(message)) == message
```
No characters, delimiters, or trailing whitespaces are discarded or modified during decomposition and reconstruction.
