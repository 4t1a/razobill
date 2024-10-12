// ==============================================================================
// Razobill Database Control-Plane Port: Backend Abstraction Interface
// ==============================================================================
//
// ARCHITECTURAL PURPOSE:
// Decouples high-level schema compaction, metadata management, and entropy auditing
// from the underlying analytical database engine.
//
// DESIGN CONTRACT:
// 1. Multi-Engine Support: Native Arrow Database Connectivity (ADBC) bridges to
//    in-process DuckDB and ClickHouse chDB without source coupling.
// 2. Flavor Abstraction: Handles engine-specific dialect nuances (UBIGINT vs UInt64,
//    string escaping, table options) via `Flavor`.
// 3. Tabular Projection: `Table` provides a strongly-typed, memory-safe in-memory
//    representation of query results with zero-copy accessors for ints, floats, and strings.
// ==============================================================================
#pragma once

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace razobill {

struct BackendError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

enum class Flavor { DuckDB, ClickHouse };

using Value = std::variant<std::monostate, std::int64_t, std::uint64_t, double, std::string>;

struct Table {
    std::vector<std::string> names;
    std::vector<std::vector<Value>> rows;

    [[nodiscard]] bool empty() const noexcept { return rows.empty(); }
    [[nodiscard]] std::int64_t i64(std::size_t r, std::size_t c) const;
    [[nodiscard]] std::uint64_t u64(std::size_t r, std::size_t c) const;
    [[nodiscard]] double f64(std::size_t r, std::size_t c) const;
    [[nodiscard]] const std::string& str(std::size_t r, std::size_t c) const;
    [[nodiscard]] bool isNull(std::size_t r, std::size_t c) const {
        return std::holds_alternative<std::monostate>(rows.at(r).at(c));
    }
    [[nodiscard]] bool is_null(std::size_t r, std::size_t c) const { return isNull(r, c); }
};

class Backend {
public:
    virtual ~Backend() = default;
    Backend() = default;
    Backend(const Backend&) = delete;
    Backend& operator=(const Backend&) = delete;

    [[nodiscard]] virtual Flavor flavor() const noexcept = 0;
    virtual void exec(std::string_view sql) = 0;
    [[nodiscard]] virtual Table query(std::string_view sql) = 0;
};

struct AdbcOptions {
    Flavor flavor = Flavor::DuckDB;
    std::string driverPath;
    std::string entrypoint;
    bool deepbind = false;
    std::vector<std::string> preloadGlobal;
    std::vector<std::pair<std::string, std::string>> databaseOptions;
    std::vector<std::string> initSql;

    // Backward compatibility mappings
    std::string driver_path;
    std::vector<std::string> preload_global;
    std::vector<std::pair<std::string, std::string>> database_options;
    std::vector<std::string> init_sql;
};

[[nodiscard]] std::unique_ptr<Backend> makeAdbcBackend(const AdbcOptions& options);

// Legacy alias
[[nodiscard]] inline std::unique_ptr<Backend> make_adbc_backend(const AdbcOptions& options) {
    return makeAdbcBackend(options);
}

} // namespace razobill
