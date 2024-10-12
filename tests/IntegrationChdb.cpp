// ==============================================================================
// Razobill ClickHouse chDB Integration & Cross-Engine Hash Parity Suite
// ==============================================================================
//
// INTEGRATION HARNESS & SCOPE:
// Validates additive mode execution and schema integration against embedded ClickHouse (chDB):
// 1. Cross-Engine Hash Parity: Verifies that ClickHouse's built-in `xxHash64()` SQL function
//    computes identical 64-bit integer hashes to `librazobill/Hash.hpp` across 134,815 events.
// 2. Additive Layout Indexing: Generates and populates `form_id` indices on raw MergeTree tables.
// 3. Dynamic Discovery: Gracefully detects whether the chDB dynamic library is available on
//    the host environment, skipping execution when running on environments without the native driver.
// ==============================================================================

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <cmath>
#include <filesystem>
#include <set>

#include "doctest.h"
#include "librazobill/Hash.hpp"
#include "librazobill/Razobill.hpp"
#include "librazobill/Slots.hpp"
#include "Support.hpp"

using namespace razobill;

namespace {
bool chdbAvailable() {
    static const bool avail = [] {
        const auto opt = support::chdbOptions();
        std::error_code ec;
        return std::filesystem::exists(opt.driverPath, ec);
    }();
    return avail;
}

void loadRaw(Backend& b) {
    b.exec("CREATE TABLE raw (sys String, level String, comp String, msg String) ENGINE = MergeTree ORDER BY tuple()");
    b.exec("INSERT INTO raw SELECT sys, level, comp, msg FROM file('/tmp/corpus.parquet', Parquet)");
}
} // namespace

TEST_CASE("chDB: additive mode, forms computed by ClickHouse SQL equal the C++ core's forms and hashes") {
    // THE HASH PARITY CONTRACT:
    // Here we assert that ClickHouse's internal `xxHash64()` function produces bit-for-bit
    // identical 64-bit integers to our C++ `core::xxh64()` implementation across 134,815 rows.
    // If even 1 single hash diverges, downstream distributed JOINs across ClickHouse clusters will break.
    if (!chdbAvailable()) {
        MESSAGE("Skipping chDB test: driver library not present");
        return;
    }
    auto b = makeAdbcBackend(support::chdbOptions());
    loadRaw(*b);
    CHECK(b->query("SELECT count() FROM raw").i64(0, 0) == 134815);
    RazobillEngine rz(*b, {.table = "raw", .purpose = "level"});
    const auto added = rz.refresh();
    CHECK(added > 0);
    CHECK(rz.refresh() == 0);

    const auto rows = b->query("SELECT msg, form_id FROM raw");
    REQUIRE(rows.rows.size() == 134815);
    core::SlotRecord s;
    std::string key;
    std::size_t bad = 0;
    std::set<std::string> coreForms;
    for (std::size_t r = 0; r < rows.rows.size(); ++r) {
        REQUIRE(core::parse(rows.str(r, 0), s) == Status::Ok);
        key = s.formKey();
        coreForms.insert(key);
        bad += core::xxh64(key) != rows.u64(r, 1);
    }
    CHECK(bad == 0);
    const auto db = b->query("SELECT form FROM rb_form");
    std::set<std::string> dbForms;
    for (std::size_t r = 0; r < db.rows.size(); ++r)
        dbForms.insert(db.str(r, 0));
    CHECK(dbForms == coreForms);
}
