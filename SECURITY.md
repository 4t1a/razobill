# Security policy

Razobill provides high-performance log decomposition in security-critical observability and data pipelines. We prioritize prompt, responsible handling of vulnerabilities.

---

## Supported versions

Security updates and bug fixes are applied to the active minor release line:

| Version line | Security status |
| --- | --- |
| `1.1.x` | Supported |
| `< 1.1.0` | End of support |

---

## Reporting a vulnerability

If you identify a security vulnerability (such as a memory safety issue, out-of-bounds read in SIMD scanners, or denial-of-service via crafted log streams):

1. **Do not open a public GitHub issue.**
2. Report the vulnerability privately through [GitHub Security Advisories](https://github.com/razobill/razobill/security/advisories/new).
3. If GitHub Advisories is inaccessible, contact the project maintainer privately.

### What to include in your report

To help us triage and patch the issue quickly, please provide:

- A clear description of the vulnerability and its potential impact.
- The affected component (`MemoryArena`, `SimdScanner`, DuckDB extension, C ABI, or `pyrazobill`).
- A minimal, reproducible test case or crafted input payload.
- Any proposed patch or mitigation.

---

## Response commitments

- **Initial acknowledgement**: Within 48 hours.
- **Triage and severity assessment**: Within 5 business days.
- **Coordinated disclosure and patch**: Within 30 days or as mutually coordinated.
