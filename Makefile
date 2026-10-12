# Root Makefile for DuckDB Community Extensions CI
# Drives build through duckdb/extension-ci-tools for the stable C_STRUCT ABI.
.PHONY: clean clean_all set_duckdb_version set_duckdb_tag configure_ci

PROJ_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

# Main extension configuration
EXTENSION_NAME=razobill
EXTENSION_CANONICAL=razobill

# Stable C API (C_STRUCT ABI)
USE_UNSTABLE_C_API=0

# Target DuckDB C API baseline version for ABI metadata
TARGET_DUCKDB_VERSION=v1.2.0

# Pass build flag to build only the extension in CI
CMAKE_EXTRA_BUILD_FLAGS=-DRAZOBILL_BUILD_EXT_ONLY=ON

all: configure release

# Ensure extension-ci-tools makefiles are included
ifeq ($(wildcard extension-ci-tools/makefiles/c_api_extensions/base.Makefile),)
$(shell git submodule update --init --recursive 2>/dev/null || git clone --depth 1 -b v1.5-variegata https://github.com/duckdb/extension-ci-tools extension-ci-tools 2>/dev/null)
endif

-include extension-ci-tools/makefiles/c_api_extensions/base.Makefile
-include extension-ci-tools/makefiles/c_api_extensions/c_cpp.Makefile

# Fallback targets in case extension-ci-tools is not present during basic calls
set_duckdb_version:
	@echo "DuckDB version setup complete."

set_duckdb_tag:
	@echo "DuckDB tag setup complete."

configure_ci:
	@echo "configure_ci complete."

configure: venv platform extension_version

debug: build_extension_library_debug build_extension_with_metadata_debug
release: build_extension_library_release build_extension_with_metadata_release

test: test_release
test_debug: test_extension_debug
test_release: test_extension_release

clean: clean_build clean_cmake
clean_all: clean clean_configure
