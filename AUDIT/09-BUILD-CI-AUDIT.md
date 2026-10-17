# 09 — Build System & CI Audit

**Scope**: `CMakeLists.txt`, `Makefile`, `.github/workflows/*.yml` (9 workflow files)

---

## 1. CI Pipeline Inventory

| Workflow | Trigger | Jobs | Coverage |
|:---------|:--------|:-----|:---------|
| `CI.yml` | push/PR to main | test-matrix (3 OS × 3 Python), AVX-512 emulation, sanitizers (ASan+UBSan, TSan), fuzzing, ABI/SBOM | **Primary gate** |
| `Sanitizers.yml` | push/PR to main | ASan+UBSan, TSan, Clang-Tidy | Memory/thread/static analysis |
| `coverage.yml` | push/PR to main | C++ LCOV + Python pytest-cov → Codecov | Coverage reporting |
| `DuckdbExtension.yml` | paths: ext/duckdb, include, src | Build + IntegrationDuckdb (3 OS) | Extension-specific |
| `Release.yml` | tag `v*.*.*` / manual | validate, build-matrix, wheels (Win/Linux/Mac), GitHub release, PyPI | Release pipeline |
| `codeql.yml` | push/PR + weekly cron | CodeQL c-cpp, python | Security scanning |
| `docs-lint.yml` | — | Documentation linting | Docs quality |
| `pages.yml` | — | GitHub Pages deployment | Docs hosting |
| `dependabot-automation.yml` | — | Auto-merge dependabot PRs | Dependency updates |

---

## 2. Findings

### F-BUILD-01 · Duplicate sanitizer jobs between `CI.yml` and `Sanitizers.yml`
| Field | Value |
|:------|:------|
| **Severity** | 🟢 LOW |
| **File** | [`CI.yml#L123-L147`](file:///C:/FM/rzd/razobill/.github/workflows/CI.yml#L123-L147), [`Sanitizers.yml`](file:///C:/FM/rzd/razobill/.github/workflows/Sanitizers.yml) |
| **Description** | `CI.yml` has an inline `sanitizers` job (ASan+UBSan, TSan) at lines 123–147. `Sanitizers.yml` has a dedicated workflow with the same ASan+UBSan and TSan jobs plus Clang-Tidy. Both run on push/PR to main. This means sanitizer tests run **twice** per push, wasting CI minutes. |
| **Impact** | Doubled CI cost for sanitizer runs. |
| **Fix** | Remove the inline `sanitizers` job from `CI.yml` and rely on the dedicated `Sanitizers.yml`. |

### F-BUILD-02 · `CI.yml` fuzzing uses non-libFuzzer standalone mode — limited coverage
| Field | Value |
|:------|:------|
| **Severity** | 🟡 MEDIUM |
| **File** | [`CI.yml#L160-L167`](file:///C:/FM/rzd/razobill/.github/workflows/CI.yml#L160-L167) |
| **Description** | The CI fuzzing job builds with `-DENABLE_FUZZING=ON` using g++ (not clang), then runs `./build-fuzz/FuzzDecomposeReconstruct 100000`. This invokes the standalone `main()` harness (random mutations, no coverage guidance). The `Sanitizers.yml` runs with `-runs=1000 -max_len=2048 -timeout=10` which suggests libFuzzer mode. However, the CI.yml job uses g++ which does not support `-fsanitize=fuzzer`. |
| **Impact** | The CI fuzzing job provides randomized stress testing but not coverage-guided fuzzing. The sanitizer workflow's libFuzzer run is only 1000 iterations — very low for meaningful fuzzing. |
| **Fix** | Increase libFuzzer iterations in `Sanitizers.yml` to at least 100,000. Add a corpus directory for persistent fuzzing artifacts. Consider using ClusterFuzz or OSS-Fuzz integration. |

### F-BUILD-03 · `Sanitizers.yml` uses unversioned `clang`/`clang++` — version drift
| Field | Value |
|:------|:------|
| **Severity** | 🟢 LOW |
| **File** | [`Sanitizers.yml#L29-L30`](file:///C:/FM/rzd/razobill/.github/workflows/Sanitizers.yml#L29-L30) |
| **Description** | `Sanitizers.yml` installs `clang llvm` (unversioned) while `CI.yml` inline sanitizer job uses `clang-18 llvm-18` (pinned). Unversioned packages resolve to whatever the runner default is, which changes as Ubuntu images update. |
| **Impact** | Inconsistent compiler versions between workflows. May mask or introduce false positives. |
| **Fix** | Pin to `clang-18 llvm-18` (or latest stable) in `Sanitizers.yml`. |

### F-BUILD-04 · No MSan (MemorySanitizer) job
| Field | Value |
|:------|:------|
| **Severity** | 🟡 MEDIUM |
| **File** | All CI workflows |
| **Description** | ASan detects buffer overflows and use-after-free. UBSan detects undefined behavior. TSan detects data races. However, **MSan** (uninitialized memory reads) is absent. MSan requires an instrumented libc++ which makes it harder to set up, but it catches a class of bugs that ASan cannot. |
| **Impact** | Uninitialized memory reads (e.g., reading uninitialized stack variables in parsing) would not be caught. |
| **Fix** | Add an MSan job with `-fsanitize=memory` and instrumented libc++ build. |

### F-BUILD-05 · `Release.yml` uses `actions/download-artifact@v8` — may not exist
| Field | Value |
|:------|:------|
| **Severity** | 🟢 LOW |
| **File** | [`Release.yml#L186`](file:///C:/FM/rzd/razobill/.github/workflows/Release.yml#L186) |
| **Description** | `actions/download-artifact@v8` is referenced. As of late 2026, v4 is the latest stable release. Using a non-existent version tag will fail at runtime. Elsewhere the workflow uses `@v7` for checkout and setup-python which also appear ahead of current stable versions. |
| **Impact** | Release pipeline would fail if these tags don't resolve. |
| **Fix** | Pin to actually released action versions (v4 for download-artifact, v4 for checkout, v5 for setup-python). |

### F-BUILD-06 · No Windows ARM64 or Linux aarch64 wheel builds
| Field | Value |
|:------|:------|
| **Severity** | 🟢 LOW |
| **File** | [`Release.yml#L140`](file:///C:/FM/rzd/razobill/.github/workflows/Release.yml#L140) |
| **Description** | Linux wheels only build `x86_64`. macOS wheels build on `macos-latest` which is ARM64 (Apple Silicon) — correct. Windows wheels only build x86_64. There are no Linux `aarch64` wheels (significant for AWS Graviton, Raspberry Pi, etc.). The CI matrix does include `ubuntu-24.04-arm` for testing, confirming ARM support works. |
| **Impact** | No pre-built wheels for Linux ARM64 users. They must build from source. |
| **Fix** | Add `CIBW_ARCHS_LINUX: "x86_64 aarch64"` with QEMU emulation or native ARM runners. |

### F-BUILD-07 · `coverage.yml` `lcov` uses `--ignore-errors mismatch,unused` — may hide real issues
| Field | Value |
|:------|:------|
| **Severity** | ⚪ NOTE |
| **File** | [`coverage.yml#L52`](file:///C:/FM/rzd/razobill/.github/workflows/coverage.yml#L52) |
| **Description** | The `lcov --capture` command uses `--ignore-errors mismatch,unused` which suppresses version mismatch warnings between gcov and lcov. This is a common workaround for CI environments but could hide genuine instrumentation problems. |
| **Impact** | Coverage numbers may be slightly inaccurate if instrumentation is mismatched. |
| **Fix** | Pin gcov and lcov versions to avoid mismatches. |

### F-BUILD-08 · `codeql.yml` references `.github/codeql/codeql-config.yml` — file not verified
| Field | Value |
|:------|:------|
| **Severity** | ⚪ NOTE |
| **File** | [`codeql.yml#L49`](file:///C:/FM/rzd/razobill/.github/workflows/codeql.yml#L49) |
| **Description** | The CodeQL init step references `config-file: .github/codeql/codeql-config.yml`. If this file does not exist, CodeQL falls back to defaults. The file was not present in the directory listing. |
| **Impact** | CodeQL may run with default rules instead of project-specific configuration. |
| **Fix** | Verify the config file exists, or remove the `config-file` parameter to use defaults explicitly. |

### F-BUILD-09 · No `cmake/librazobillConfig.cmake.in` for find_package support
| Field | Value |
|:------|:------|
| **Severity** | 🟢 LOW |
| **File** | `CMakeLists.txt` |
| **Description** | There is no CMake config file template for downstream C++ consumers. Users who want to use `find_package(librazobill)` cannot do so. The library is primarily consumed via the C ABI, DuckDB extension, or Python wheel, so this is lower priority. |
| **Impact** | C++ consumers must manually specify include paths and link flags. |
| **Fix** | Add `cmake/librazobillConfig.cmake.in` and install targets. |

---

## 3. Summary

| ID | Severity | Component | One-liner |
|:---|:---------|:----------|:----------|
| F-BUILD-01 | 🟢 LOW | CI | Duplicate sanitizer jobs waste CI minutes |
| F-BUILD-02 | 🟡 MEDIUM | CI | libFuzzer iterations too low (1000) |
| F-BUILD-03 | 🟢 LOW | CI | Unversioned clang in Sanitizers.yml |
| F-BUILD-04 | 🟡 MEDIUM | CI | No MSan (MemorySanitizer) job |
| F-BUILD-05 | 🟢 LOW | CI | `download-artifact@v8` may not exist |
| F-BUILD-06 | 🟢 LOW | Release | No Linux aarch64 wheel builds |
| F-BUILD-07 | ⚪ NOTE | CI | lcov error suppression flags |
| F-BUILD-08 | ⚪ NOTE | CI | CodeQL config file reference unverified |
| F-BUILD-09 | 🟢 LOW | Build | No CMake find_package support |
