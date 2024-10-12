# Security Policy

## Supported Versions

| Version | Supported          |
| ------- | ------------------ |
| 1.0.x   | :white_check_mark: |
| < 1.0.0 | :x:                |

---

## Reporting a Vulnerability

The Razobill engineering team takes the security and integrity of our parsing engine and database extensions seriously. If you discover a security vulnerability (such as memory corruption, buffer overflow in SIMD scanners, or denial-of-service via crafted log streams):

1. **Do NOT open a public GitHub issue.**
2. Send an email to the security team or open a private GitHub Security Advisory on the repository.
3. Include the following details:
   - Type of issue (e.g., buffer overflow, out-of-bounds read, integer truncation).
   - Component affected (`MemoryArena`, `SimdScanner`, DuckDB extension, Python C-Extension).
   - A minimal reproducible test case or crafted input payload.
   - Any proposed patch or fix.

### Response Timeline
- **Acknowledgement**: Within 48 hours of initial report.
- **Assessment & Triage**: Within 5 business days.
- **Coordinated Disclosure & Patch Release**: Within 30 days or as mutually agreed.
