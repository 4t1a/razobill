#pragma once
// Shared by integration tests: backend factories and test result reporting.
//
// INTEGRATION HARNESS:
// Dynamically discovers and boots embedded DuckDB and ClickHouse chDB runtimes
// via Arrow Database Connectivity (ADBC).
// Handles path normalization (Windows '\\' -> '/') so SQL `LOAD` statements work seamlessly.
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

#include "librazobill/Backend.hpp"

namespace support {

inline const char* env(const char* k, const char* d) {
    const char* v = std::getenv(k);
    return v ? v : d;
}

// Forward-slash path normalizer: Windows loves backslashes, but SQL parsers treat them as escape characters!
inline std::string normalizePath(std::string p) {
    for (char& c : p)
        if (c == '\\') c = '/';
    return p;
}

inline razobill::AdbcOptions duckdbOptions() {
    razobill::AdbcOptions o;
    o.flavor = razobill::Flavor::DuckDB;
#if defined(_WIN32)
    const char* defLib = "duckdb.dll";
    const char* forkeLib = "C:/Users/Forke/AppData/Roaming/Python/Python313/site-packages/_duckdb.cp313-win_amd64.pyd";
    const char* defExt = "build/librazobill.duckdb_extension";
    const char* chosenLib = defLib;
    if (std::getenv("RZB_DUCKDB_LIB")) {
        chosenLib = std::getenv("RZB_DUCKDB_LIB");
    } else {
        std::error_code ec;
        if (std::filesystem::exists(forkeLib, ec)) {
            chosenLib = forkeLib;
        }
    }
#elif defined(__APPLE__)
    const char* defLib = "libduckdb.dylib";
    const char* defExt = "build/librazobill.duckdb_extension";
    const char* chosenLib = env("RZB_DUCKDB_LIB", defLib);
#else
    const char* defLib = "/usr/local/lib/libduckdb.so";
    const char* defExt = "build/librazobill.duckdb_extension";
    const char* chosenLib = env("RZB_DUCKDB_LIB", defLib);
#endif
    o.driverPath = o.driver_path = normalizePath(chosenLib);
    o.entrypoint = "duckdb_adbc_init";
    o.databaseOptions = o.database_options = {{"path", ":memory:"}, {"allow_unsigned_extensions", "true"}};
    o.initSql = o.init_sql = {std::string("LOAD '") + normalizePath(env("RZB_EXT", defExt)) + "'"};
    return o;
}

inline razobill::AdbcOptions chdbOptions() {
    razobill::AdbcOptions o;
    o.flavor = razobill::Flavor::ClickHouse;
    o.driverPath = o.driver_path = env("RZB_CHDB_LIB", "/usr/local/lib/python3.12/dist-packages/chdb/_chdb.abi3.so");
    o.entrypoint = "AdbcDriverInit";
    o.preloadGlobal = o.preload_global = {env("RZB_LIBPYTHON", "/lib/x86_64-linux-gnu/libpython3.12.so.1.0")};
    o.deepbind = true;
    return o;
}

inline void result(const char* name, double v) {
    std::printf("RESULT %s %.12g\n", name, v);
    std::fflush(stdout);
}

// Backward compatibility aliases
inline razobill::AdbcOptions duckdb_options() {
    return duckdbOptions();
}
inline razobill::AdbcOptions chdb_options() {
    return chdbOptions();
}
inline std::string normalize_path(std::string p) {
    return normalizePath(p);
}

} // namespace support
