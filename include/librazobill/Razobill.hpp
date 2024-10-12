// Razobill control plane: forms, gated meaning, versioned rules, compaction, and dictionary lifecycle.
#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "librazobill/Backend.hpp"
#include "librazobill/Dialect.hpp"
#include "librazobill/Scanner.hpp"
#include "librazobill/Stats.hpp"

namespace razobill {

struct Config {
    std::string table; // raw table (additive mode) or compact family name (compact mode)
    std::string messageColumn = "msg";
    std::string purpose; // column the meaning must stay adequate for
    double tau = 1.0;    // confidence threshold
    double eps = 0.05;   // gate threshold in bits lost per merged event
    core::ScanMode scanMode = core::ScanMode::Semantic;

    // Backward compatibility mappings
    std::string message_column;
    core::ScanMode scan_mode = core::ScanMode::Semantic;
};

struct GateRefused : std::runtime_error {
    GateRefused(const std::string& what, double jsd_, double share_)
        : std::runtime_error(what), jsd(jsd_), share(share_) {}
    double jsd, share;
};

struct Defined {
    int version;
    std::size_t forms;
    double lossPerMergedEvent;
    double lossTableWide;

    // Backward compatibility mappings
    double loss_per_merged_event = 0;
    double loss_table_wide = 0;
};

struct MeaningOf {
    std::uint64_t formId;
    std::optional<std::string> meaning; // nullopt if unresolved or ambiguous
    std::optional<double> doubt;        // nullopt if unresolved

    // Backward compatibility mappings
    std::uint64_t form_id = 0;
};

struct DictionaryStats {
    std::size_t totalBefore = 0;
    std::size_t activeAfter = 0;
    std::size_t purgedCount = 0;

    // Backward compatibility mappings
    std::size_t total_before = 0;
    std::size_t active_after = 0;
    std::size_t purged_count = 0;
};

class RazobillEngine {
public:
    RazobillEngine(Backend& backend, Config config);

    // ---- Storage ----
    // Additive mode: returns the count of newly discovered forms
    std::size_t refresh();

    // Compact mode (DuckDB): append src to <table>_c, expose through view <table>
    std::uint64_t compact(const std::string& src);

    // Verifies rebuilt view matches raw src byte-for-byte; 0 means exact lossless reproduction
    std::uint64_t verify(const std::string& src);

    // ---- Text Dictionary Lifecycle ----
    // Purges unreferenced string entries from rb_str
    DictionaryStats gcDictionary();

    // Compacts the string dictionary to dense contiguous IDs [0, N-1] and remaps referencing columns
    void compactDictionary();

    // Legacy aliases
    DictionaryStats gc_dictionary() { return gcDictionary(); }
    void compact_dictionary() { compactDictionary(); }

    // ---- Semantic Meaning & Information Gate ----
    Defined define(const std::string& meaning, const std::string& pattern, double weight = 1.0, bool force = false);
    int retire(const std::string& meaning);
    void pin(int version);
    [[nodiscard]] int version();
    [[nodiscard]] stats::Ledger ledger();
    [[nodiscard]] double leakage();
    [[nodiscard]] std::optional<MeaningOf> meaningOf(const std::string& text);
    [[nodiscard]] std::optional<MeaningOf> meaning_of(const std::string& text) { return meaningOf(text); }

    // Unresolved forms ranked by event frequency
    [[nodiscard]] std::vector<std::pair<std::string, std::int64_t>> todo(std::size_t n = 50);

private:
    struct Rule {
        std::string meaning, pattern;
        double weight;
    };
    struct FormP {
        std::uint64_t form;
        std::string meaning;
        double p;
    };

    void installLocked();
    void detectLocked();
    void requireReadyLocked() const;
    std::size_t registerFormsLocked(const std::string& selectForms);
    std::vector<Rule> activeRulesLocked(int version);
    std::vector<FormP> resolveLocked(const std::vector<Rule>& rules);
    void rebuildDerivedLocked();
    void createViewsLocked();
    std::vector<stats::XY> xyLocked(std::unordered_map<std::string, std::uint32_t>& yids);
    double totalEventsLocked();
    std::string physKeyLocked() const;

    Backend& db_;
    Config cfg_;
    std::unique_ptr<Dialect> dx_;
    std::mutex mu_;
    std::string phys_;
    std::string view_;
    bool compact_ = false;
    bool ready_ = false;
};

using Razobill = RazobillEngine;

} // namespace razobill
