# Release Governance & Versioning Specification

This document defines the formal engineering contract for versioning, binary compatibility, release engineering, and supply-chain provenance for **`librazobill`** and its multi-language client ecosystem (`pyrazobill`, etc.).

---

## 1. SemVer 2.0.0 Architectural Contract

`librazobill` adheres strictly to **Semantic Versioning 2.0.0**:

```
                       MAJOR . MINOR . PATCH
                          │       │       │
 Breaking Changes ────────┘       │       └── Backward-Compatible Bug Fixes
                                  │
    Backward-Compatible Features ─┘
```

| Increment | Trigger | Guarantee |
|---|---|---|
| **MAJOR** | Breaking API changes, struct layout shifts, symbol removal, C ABI breakage | Requires explicit consumer code update or migration. |
| **MINOR** | Backward-compatible features, new dialect adapters, new foreign language SDKs | Safe drop-in upgrade. Zero consumer breakage. |
| **PATCH** | Performance enhancements, security patches, compiler warnings resolution | Mandatory auto-upgrade candidate. Identical interface. |

---

## 2. API vs. ABI Stability Model

Because `librazobill` compiles to static libraries (`liblibrazobill.a`), shared libraries (`librazobill.dll`), DuckDB extensions (`librazobill.duckdb_extension`), and CPython extension modules (`Core.pyd`), we maintain distinct stability guarantees:

### A. Source API Stability (C++20 & Python)
- Public symbols exposed in `include/librazobill/*.hpp` and `lib/python/pyrazobill` will remain source-compatible across all `1.x.y` releases.
- Deprecated methods emit compile-time `[[deprecated]]` notices and runtime warnings in at least one MINOR release cycle prior to elimination in the next MAJOR release.

### B. Binary ABI Stability (Universal C ABI)
- The C ABI defined in `include/librazobill/RazobillC.h` carries a dedicated `LIBRAZOBILL_ABI_VERSION` integer (currently `1`).
- Struct `RzSlotRecord` has deterministic field order, fixed pointer widths, and compiler-verified static assertions.
- Any change to field ordering, alignment, or calling conventions strictly forces a MAJOR version increment (`LIBRAZOBILL_ABI_VERSION = 2`).

---

## 3. Single Source of Truth Versioning Pipeline

To eliminate manual desynchronization across languages and tools:

```
[ VERSION file ] (e.g. "1.0.0")
       │
       ├───> CMake (project version, SOVERSION, targets)
       ├───> include/librazobill/Version.hpp (C++ compile-time macros & VersionInfo)
       ├───> include/librazobill/RazobillC.h (C ABI RzVersion() exports)
       ├───> tools/AppendExtensionMetadata.py (DuckDB extension header signature)
       └───> lib/python/pyrazobill/__init__.py (pyrazobill.__version__)
```

1. Maintainers bump the string in the root `VERSION` file.
2. Build tools automatically synchronize the version into binaries, headers, and package metadata.

---

## 4. Conventional Commits & Automated Release Engineering

All Git commit messages must follow the [Conventional Commits 1.0.0](https://www.conventionalcommits.org/) specification:

- `fix(core): fix trailing null byte in token scanner` $\rightarrow$ triggers **PATCH** bump
- `feat(dialect): add ClickHouse 26 support` $\rightarrow$ triggers **MINOR** bump
- `feat(abi)!: redesign SlotRecord memory layout` $\rightarrow$ triggers **MAJOR** bump

Automated CI/CD tooling (`release-please` / `semantic-release`) parses conventional commits, increments `VERSION`, updates `CHANGELOG.md`, creates a signed Git tag (`vMAJOR.MINOR.PATCH`), and triggers multi-platform binary compilation.

---

## 5. Software Bill of Materials (SBOM) & Provenance

Every official release produces:
1. **SPDX 2.3 SBOM** ([`docs/governance/SbomSpdx.json`](SbomSpdx.json)) listing every compiled and vendored dependency (`adbc`, `duckdbcapi`, `doctest`).
2. **SLSA Level 3 Build Attestation**: Linking each released binary artifact directly to the immutable Git commit SHA, compiler hash, and CI/CD builder identity.
