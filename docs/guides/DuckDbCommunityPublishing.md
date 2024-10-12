# Publishing to DuckDB Community Extensions (Channel A)

This guide provides the complete, step-by-step procedure for publishing `razobill` to the official **DuckDB Community Extensions** repository (`duckdb/community-extensions`).

Once merged, anyone in the world using DuckDB can install and load your extension using standard SQL:
```sql
INSTALL razobill FROM community;
LOAD razobill;
```

---

## 1. Overview of the DuckDB Community Process

DuckDB provides an official community repository at [github.com/duckdb/community-extensions](https://github.com/duckdb/community-extensions).

Instead of requiring authors to compile and distribute binaries for every operating system, DuckDB's centralized automated CI infrastructure does it for you:
1. You submit a Pull Request adding a single metadata descriptor file: `extensions/razobill/description.yml`.
2. When the PR is merged, DuckDB's automated build matrix compiles, tests, and cryptographically signs `razobill.duckdb_extension` for all supported platforms:
   - `linux_amd64`, `linux_arm64`, `linux_amd64_gcc4`
   - `osx_amd64`, `osx_arm64` (Apple Silicon)
   - `windows_amd64`
   - `wasm_mvp`, `wasm_eh`, `wasm_threads` (WebAssembly)
3. The signed binaries are automatically published to DuckDB's official S3 distribution repository (`community-extensions.duckdb.org`).

---

## 2. Prerequisites Before Submitting

1. **Zenodo Priority & Public Research Repository**:
   - The foundational scientific paper is officially registered on **Zenodo**: DOI [10.5281/zenodo.23287286](https://doi.org/10.5281/zenodo.23287286) ([zenodo.org/records/23287286](https://zenodo.org/records/23287286)) with research code at [github.com/4t1a/Latitude-Doubt](https://github.com/4t1a/Latitude-Doubt).
   - Publish your `razobill` repository publicly on GitHub (e.g. `https://github.com/razobill/razobill`).
2. **Release Tag**:
   - Tag the release commit (e.g. `v1.0.0`) and push the tag to GitHub:
     ```bash
     git tag v1.0.0
     git push origin v1.0.0
     ```

---

## 3. Step-by-Step Submission Instructions

### Step 1: Fork the Community Extensions Repository
Navigate to [https://github.com/duckdb/community-extensions](https://github.com/duckdb/community-extensions) and click **Fork**.

Clone your fork locally:
```bash
git clone https://github.com/<your-username>/community-extensions.git
cd community-extensions
```

### Step 2: Create the Extension Folder
Create a dedicated folder for `razobill` under `extensions/`:
```bash
mkdir -p extensions/razobill
```

### Step 3: Add `description.yml`
Copy the pre-configured descriptor from `razobill/ext/duckdb/description.yml` into your fork:

```yaml
extension:
  name: razobill
  description: High-performance lossless log parsing, analytical database compaction, and information-theoretic semantic accounting.
  version: 1.0.0
  language: C++
  build: cmake
  license: Apache-2.0
  maintainers:
    - Forke
  repo:
    github: razobill/razobill
  ref: v1.0.0
```

*Note: Replace `maintainers` with your actual GitHub username, and ensure `ref` points to the published release tag (`v1.0.0`) or commit hash.*

### Step 4: Commit & Open the Pull Request
```bash
git checkout -b add-razobill-extension
git add extensions/razobill/description.yml
git commit -m "Add razobill extension"
git push origin add-razobill-extension
```

Go to [https://github.com/duckdb/community-extensions/pulls](https://github.com/duckdb/community-extensions/pulls) and click **New Pull Request**.

Title: `Add razobill extension`
Description:
```markdown
### Summary
Submitting `razobill` - a high-performance C++20 extension for lossless log parsing, analytical database compaction (45%+ storage savings), and information-theoretic semantic accounting.

- Repository: https://github.com/razobill/razobill
- License: Apache-2.0
- ABI Type: C_STRUCT (DuckDB C API)
```

---

## 4. What Happens Next

1. **Automated CI Validation**:
   - DuckDB's GitHub Actions CI will automatically trigger on your Pull Request.
   - It will clone `razobill/razobill`, run CMake, compile the C API extension across Linux, macOS, and Windows, and test loading.
2. **Review & Merge**:
   - A DuckDB core team member will review the descriptor and merge the PR.
3. **Availability**:
   - Within minutes of merging, `razobill` becomes available worldwide across all supported platforms.
   - Any user can immediately run:
     ```sql
     INSTALL razobill FROM community;
     LOAD razobill;
     ```

---

## 5. Releasing Future Updates

When you release a new version of Razobill (e.g. `v1.1.0`):
1. Tag your new version on GitHub (`v1.1.0`).
2. Open a small PR to `duckdb/community-extensions` updating:
   ```yaml
   version: 1.1.0
   ref: v1.1.0
   ```
3. Once merged, DuckDB will automatically build and distribute the new binaries for all platforms.
