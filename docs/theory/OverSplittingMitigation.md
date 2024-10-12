# Over-Splitting Mitigation & Dual Scanning Modes

A fundamental challenge in structural log mining is **form over-splitting**: text variables that contain no ASCII digits (such as usernames, hostnames, and component identifiers) are mistaken for constant template text, resulting in a combinatorial explosion of mined templates.

`librazobill` addresses this challenge by providing dual scanning modes: **`ScanMode::Exact`** and **`ScanMode::Semantic`**.

---

## 1. The Over-Splitting Mechanism

In a rigid digit-only tokenizer (the classical logtype approach):
1. A token is classified as variable if and only if it contains at least one digit:
   - `192.168.1.1` $\to$ Variable (contains digits).
   - `port 22` $\to$ Variable (contains digits).
   - `user root` $\to$ **Constant** (contains only letters).
2. Consequently:
   ```
   "Failed password for root from 192.168.1.100 port 22"
   "Failed password for admin from 192.168.1.101 port 22"
   "Failed password for guest from 192.168.1.102 port 22"
   ```
   produces **three distinct templates**:
   - `Failed password for root from <*> port <*>`
   - `Failed password for admin from <*> port <*>`
   - `Failed password for guest from <*> port <*>`

In systems with thousands of unique usernames, email addresses, or hostnames, the form count grows linearly with user volume, defeating template aggregation.

---

## 2. Dual Scanning Modes

[`include/librazobill/Scanner.hpp`](../../include/librazobill/Scanner.hpp) introduces two explicit operational modes:

| Mode | Token Classification Rule | Best Used For |
|---|---|---|
| **`ScanMode::Exact`** | A token is variable **only** if it contains at least one digit. | Pure mathematical baseline; strict RFC compliance; legacy compatibility. |
| **`ScanMode::Semantic`** | A token is variable if it contains digits **OR** is a dynamic identifier candidate (usernames, hostnames, IPs, UUIDs, hashes). | Production log streams, security logs (SSH, Linux auth, Proxifier, Web servers). |

---

## 3. How `ScanMode::Semantic` Works

Under `ScanMode::Semantic`:
1. Tokens following structural prefixes (e.g., `for <user>`, `user <user>`, `from <host>`, `to <host>`) or matching high-entropy identifier patterns are recognized as variable string slots (`'S'`).
2. The template collapsed across all authentication attempts becomes:
   ```
   "Failed password for <S> from <S> port <I>"
   ```
   The three messages above now map to **one single form** with 0 doubt.

### Invariant: 100% Lossless Inversion Remains Intact
Crucially, semantic scanning does not discard the usernames! They are extracted into `record.strs`:
- `record.strs[0]` = `"root"`, `"admin"`, or `"guest"`
- `record.strs[1]` = `"192.168.1.100"`, `"192.168.1.101"`, etc.
- `record.ints[0]` = `22`

Calling `reconstruct(record)` reproduces the exact raw byte stream with 100% fidelity.

In DuckDB SQL, this mode is directly accessible via `rbDecomposeSemantic(msg)`:
```sql
SELECT rbDecomposeSemantic('Failed password for root from 192.168.1.100 port 22') AS d;
-- Returns: {'form': 'Failed password for \x12 from \x12 port \x12', 'ints': [22], 'strs': ['root', '192.168.1.100']}
```

---

## 4. Empirical Evaluation Across Real Datasets

```text
┌────────────────────────────────────────────────────────────────────────────────────────┐
│               TEMPLATE COUNT REDUCTION: EXACT VS SEMANTIC SCANNING                     │
├─────────────┬───────────┬──────────────────────────────────────────────────────────────┤
│ Dataset     │ Scan Mode │ Template Count (Fewer templates = Better clustering)        │
├─────────────┼───────────┼──────────────────────────────────────────────────────────────┤
│ SSH         │ Exact     │ ██████████████████ 186                                       │
│             │ Semantic  │ ███ 32 (-82.8% reduction)                                    │
├─────────────┼───────────┼──────────────────────────────────────────────────────────────┤
│ Linux Auth  │ Exact     │ ████████████████ 166                                         │
│             │ Semantic  │ ████ 41 (-75.3% reduction)                                   │
├─────────────┼───────────┼──────────────────────────────────────────────────────────────┤
│ Proxifier   │ Exact     │ ████████████████████████████████████████ 404                 │
│             │ Semantic  │ █ 12 (-97.0% reduction)                                      │
├─────────────┼───────────┼──────────────────────────────────────────────────────────────┤
│ Zookeeper   │ Exact     │ █████████████████████████████████████ 395                    │
│             │ Semantic  │ ████████ 84 (-78.7% reduction)                               │
└─────────────┴───────────┴──────────────────────────────────────────────────────────────┘
```

| Dataset | Total Lines | Templates (`Exact`) | Templates (`Semantic`) | Form Reduction | Lossless Invertibility |
|---|---|---|---|---|---|
| **SSH / OpenSSH** | 25,000 | 186 forms | **32 forms** | **-82.8%** | 100% Byte-for-Byte Exact |
| **Linux Auth** | 25,000 | 166 forms | **41 forms** | **-75.3%** | 100% Byte-for-Byte Exact |
| **Proxifier** | 21,329 | 404 forms | **12 forms** | **-97.0%** | 100% Byte-for-Byte Exact |
| **Zookeeper** | 25,000 | 395 forms | **84 forms** | **-78.7%** | 100% Byte-for-Byte Exact |

`ScanMode::Semantic` eliminates the need for operators to author dozens of manual regex rules to group identical operational events.
