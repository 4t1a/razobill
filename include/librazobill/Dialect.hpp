// SQL dialect strategy for DuckDB and ClickHouse engines.
#pragma once

#include <memory>
#include <string>
#include <string_view>

#include "librazobill/Backend.hpp"
#include "librazobill/Scanner.hpp"

namespace razobill {

class Dialect {
public:
    virtual ~Dialect() = default;

    [[nodiscard]] virtual Flavor flavor() const noexcept = 0;
    [[nodiscard]] virtual bool supportsCompact() const noexcept = 0;
    [[nodiscard]] virtual bool denseFormKey() const noexcept = 0;

    [[nodiscard]] virtual std::string quote(std::string_view s) const = 0;
    [[nodiscard]] virtual std::string tU64() const = 0;
    [[nodiscard]] virtual std::string tI32() const = 0;
    [[nodiscard]] virtual std::string tF64() const = 0;
    [[nodiscard]] virtual std::string tStr() const = 0;
    [[nodiscard]] virtual std::string tNullableStr() const = 0;
    [[nodiscard]] virtual std::string tableTail(std::string_view orderBy) const = 0;
    [[nodiscard]] virtual std::string toText(std::string_view expr) const = 0;
    [[nodiscard]] virtual std::string matches(std::string_view haystack, std::string_view patternLiteral) const = 0;
    [[nodiscard]] virtual std::string formExpr(std::string_view col,
                                               core::ScanMode mode = core::ScanMode::Semantic) const = 0;
    [[nodiscard]] virtual std::string hashExpr(std::string_view text) const = 0;
    [[nodiscard]] virtual std::string templateExpr(std::string_view form) const = 0;
    [[nodiscard]] virtual std::string truncate(std::string_view table) const = 0;

    // Legacy compatibility methods
    [[nodiscard]] bool supports_compact() const noexcept { return supportsCompact(); }
    [[nodiscard]] bool dense_form_key() const noexcept { return denseFormKey(); }
    [[nodiscard]] std::string t_u64() const { return tU64(); }
    [[nodiscard]] std::string t_i32() const { return tI32(); }
    [[nodiscard]] std::string t_f64() const { return tF64(); }
    [[nodiscard]] std::string t_str() const { return tStr(); }
    [[nodiscard]] std::string t_nullable_str() const { return tNullableStr(); }
    [[nodiscard]] std::string table_tail(std::string_view orderBy) const { return tableTail(orderBy); }
    [[nodiscard]] std::string to_text(std::string_view expr) const { return toText(expr); }
    [[nodiscard]] std::string form_expr(std::string_view col, core::ScanMode mode = core::ScanMode::Semantic) const {
        return formExpr(col, mode);
    }
    [[nodiscard]] std::string hash_expr(std::string_view text) const { return hashExpr(text); }
    [[nodiscard]] std::string template_expr(std::string_view form) const { return templateExpr(form); }
};

[[nodiscard]] std::unique_ptr<Dialect> makeDialect(Flavor f);

// Legacy alias
[[nodiscard]] inline std::unique_ptr<Dialect> make_dialect(Flavor f) {
    return makeDialect(f);
}

} // namespace razobill
