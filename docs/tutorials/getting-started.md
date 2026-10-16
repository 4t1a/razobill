# Getting started in 5 minutes

Learn how to split log lines into structured templates and reconstruct the exact original strings.
Read this guide if you are using Razobill for the first time.
Skip this guide if you already have Razobill installed and running in your pipeline.

---

## What you will build

You will parse unstructured log lines into structured templates, numbers, and words.
Then, you will verify bit-exact reconstruction of the original text.

---

## Step 1: Install the library

Install the Python client package using `pip`:

```bash
pip install pyrazobill
```

Verify that the package is installed and check its version:

```bash
python -c "import pyrazobill as rz; print(rz.__version__)"
```

Expected output:

```text
1.1.2
```

---

## Step 2: Split your first log line

Open a Python interactive shell and run this code:

```python
import pyrazobill as rz

# Sample authentication failure event
line = "Failed password for root from 192.168.1.100 port 22"
record = rz.RzSplit(line)

print("Template:", record.template)
print("Numbers: ", record.ints)
print("Words:   ", record.strs)
```

Expected output:

```text
Template: Failed password for {s} from {s} port {i}
Numbers:  [22]
Words:    ['root', '192.168.1.100']
```

The split operation extracted the constant string skeleton into a template.
Variable components were typed into numbers (`{i}`) and words (`{s}`).

---

## Step 3: Reconstruct the original bytes

Pass the decomposed record back into `RzJoin`:

```python
reconstructed = rz.RzJoin(*record)
print("Reconstructed:", reconstructed)
print("Matches input:", reconstructed == line)
```

Expected output:

```text
Reconstructed: Failed password for root from 192.168.1.100 port 22
Matches input: True
```

> [!NOTE]
> **TL;DR**: `RzJoin(*RzSplit(x)) == x` is guaranteed for all valid inputs. No whitespace, delimiter, or casing is modified.

---

## Step 4: Parse with DuckDB SQL

If you work with DuckDB, you can run the same logic in SQL:

```sql
-- Load the extension
INSTALL razobill FROM community;
LOAD razobill;

-- Decompose into structured columns
SELECT RzSplit('Failed password for root from 192.168.1.100 port 22') AS parsed;
```

Expected output:

```text
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                                         parsed                                         │
│                 struct(template varchar, ints bigint[], strs varchar[])                │
├────────────────────────────────────────────────────────────────────────────────────────┤
│ {'template': 'Failed password for {s} from {s} port {i}', 'ints': [22], 'strs': [root… │
└────────────────────────────────────────────────────────────────────────────────────────┘
```

---

## Next steps

- Explore batch processing in the [Python developer guide](../how-to/python.md).
- Learn analytical SQL queries in the [DuckDB extension guide](../how-to/duckdb.md).
- Read the mathematical guarantee in [Lossless invertibility](../explanation/lossless-invertibility.md).
