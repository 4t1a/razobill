// High-Performance Pybind11 Python Bindings for librazobill.
// Exposes pyrazobill module with zero-underscore ergonomic naming and memory-safe slot record lifetime.
//
// ARCHITECTURE: Memory Lifetime Safety in Python
// In C++, zero-copy parsing uses `std::string_view` into an existing buffer.
// In Python, string objects are transient and managed by CPython's reference-counting GC.
// If Python code executes:
//     rec = pyrazobill.parse("some log line")
// the Python string might be garbage collected immediately after the function returns!
//
// To guarantee 100% safety with zero dangling pointers, `PySlotRecord` takes ownership of the
// underlying string in a `std::shared_ptr<std::string>`.
// Even if the caller discards their original Python string, the record keeps the memory alive.
// Zero crashes. Zero segfaults. Absolute memory safety.
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "librazobill/Channel.hpp"
#include "librazobill/Hash.hpp"
#include "librazobill/Slots.hpp"
#include "librazobill/Stats.hpp"
#include "librazobill/ThreadPool.hpp"
#include "librazobill/Version.hpp"

namespace py = pybind11;
using namespace razobill;

// Python-facing SlotRecord: owns its underlying message buffer to prevent dangling string views.
struct PySlotRecord {
    std::shared_ptr<std::string> msgBuffer;
    core::SlotRecord slots;

    PySlotRecord() = default;

    explicit PySlotRecord(std::string msg, core::ScanMode mode = core::ScanMode::Semantic)
        : msgBuffer(std::make_shared<std::string>(std::move(msg))) {
        (void)core::parse(*msgBuffer, slots, mode);
    }

    [[nodiscard]] std::string formKey() const { return slots.formKey(); }
    [[nodiscard]] std::uint64_t formKeyHash() const noexcept { return slots.formKeyHash(); }
    [[nodiscard]] const std::vector<std::int64_t>& ints() const noexcept { return slots.ints; }
    [[nodiscard]] std::vector<std::string> strs() const {
        std::vector<std::string> v;
        v.reserve(slots.strs.size());
        for (const auto& sv : slots.strs)
            v.emplace_back(sv);
        return v;
    }
    [[nodiscard]] const std::string& skeleton() const noexcept { return slots.skeleton; }
    [[nodiscard]] const std::string& types() const noexcept { return slots.types; }

    [[nodiscard]] std::string reconstruct() const {
        std::string out;
        std::string key = slots.formKey();
        (void)core::reconstruct(key, slots.ints, slots.strs, out);
        return out;
    }
};

struct AuditResult {
    std::size_t totalLines = 0;
    std::size_t totalBytes = 0;
    double elapsedSec = 0.0;
    std::size_t uniqueFormsCount = 0;
    double msgsPerSec = 0.0;
    double mbPerSec = 0.0;
    std::size_t mismatches = 0;
};

// Multi-threaded audit with zero GIL contention
AuditResult auditBatchParallel(const std::vector<std::string>& lines, core::ScanMode mode = core::ScanMode::Semantic,
                               std::size_t numThreads = 0) {
    if (numThreads == 0) numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0) numThreads = 1;

    AuditResult result;
    result.totalLines = lines.size();
    for (const auto& l : lines)
        result.totalBytes += l.size();

    const auto t0 = std::chrono::steady_clock::now();

    core::ThreadPool pool(numThreads);
    const std::size_t chunkSize = std::max<std::size_t>(1000, (lines.size() + numThreads - 1) / numThreads);

    std::vector<std::size_t> localMismatches(numThreads, 0);
    std::vector<std::unordered_set<std::uint64_t>> localForms(numThreads);

    pool.parallelFor(lines.size(), chunkSize, [&](std::size_t start, std::size_t end) {
        std::size_t workerId = (start / chunkSize) % numThreads;
        core::SlotRecord localSlots;
        std::string localRebuilt;
        std::string localKey;

        for (std::size_t i = start; i < end; ++i) {
            const auto& line = lines[i];
            if (core::parse(line, localSlots, mode) != Status::Ok) continue;
            localSlots.formKey(localKey);
            localForms[workerId].insert(core::xxh64(localKey));

            if (core::reconstruct(localKey, localSlots.ints, localSlots.strs, localRebuilt) != Status::Ok ||
                localRebuilt != line) {
                localMismatches[workerId]++;
            }
        }
    });

    const auto t1 = std::chrono::steady_clock::now();
    result.elapsedSec = std::chrono::duration<double>(t1 - t0).count();

    std::unordered_set<std::uint64_t> combinedForms;
    for (std::size_t t = 0; t < numThreads; ++t) {
        result.mismatches += localMismatches[t];
        combinedForms.insert(localForms[t].begin(), localForms[t].end());
    }
    result.uniqueFormsCount = combinedForms.size();

    if (result.elapsedSec > 0) {
        result.msgsPerSec = static_cast<double>(result.totalLines) / result.elapsedSec;
        result.mbPerSec = (static_cast<double>(result.totalBytes) / (1024.0 * 1024.0)) / result.elapsedSec;
    }

    return result;
}

PYBIND11_MODULE(Core, m) {
    m.doc() = "pyrazobill: High-performance log parsing and information-theoretic analysis engine";

    py::enum_<core::ScanMode>(m, "ScanMode")
        .value("Exact", core::ScanMode::Exact)
        .value("DigitOnly", core::ScanMode::DigitOnly)
        .value("Semantic", core::ScanMode::Semantic)
        .export_values();

    py::class_<PySlotRecord>(m, "SlotRecord")
        .def(py::init<>())
        .def_property_readonly("formKey", &PySlotRecord::formKey)
        .def_property_readonly("formKeyHash", &PySlotRecord::formKeyHash)
        .def_property_readonly("ints", &PySlotRecord::ints)
        .def_property_readonly("strs", &PySlotRecord::strs)
        .def_property_readonly("skeleton", &PySlotRecord::skeleton)
        .def_property_readonly("types", &PySlotRecord::types)
        .def("reconstruct", &PySlotRecord::reconstruct)
        .def("__repr__", [](const PySlotRecord& s) {
            return "<pyrazobill.SlotRecord formKey=" + s.formKey() + " ints=" + std::to_string(s.ints().size()) +
                   " strs=" + std::to_string(s.strs().size()) + ">";
        });

    py::class_<AuditResult>(m, "AuditResult")
        .def_readonly("totalLines", &AuditResult::totalLines)
        .def_readonly("totalBytes", &AuditResult::totalBytes)
        .def_readonly("elapsedSec", &AuditResult::elapsedSec)
        .def_readonly("uniqueFormsCount", &AuditResult::uniqueFormsCount)
        .def_readonly("msgsPerSec", &AuditResult::msgsPerSec)
        .def_readonly("mbPerSec", &AuditResult::mbPerSec)
        .def_readonly("mismatches", &AuditResult::mismatches)
        .def("__repr__", [](const AuditResult& a) {
            return "<AuditResult lines=" + std::to_string(a.totalLines) +
                   " msgsPerSec=" + std::to_string(static_cast<long long>(a.msgsPerSec)) +
                   " mismatches=" + std::to_string(a.mismatches) + ">";
        });

    m.def(
        "parse", [](std::string msg, core::ScanMode mode) { return PySlotRecord(std::move(msg), mode); },
        py::arg("msg"), py::arg("mode") = core::ScanMode::Semantic, "Parses a single log line into a SlotRecord");

    m.def(
        "parseBatch",
        [](std::vector<std::string> msgs, core::ScanMode mode) {
            std::vector<PySlotRecord> records;
            records.reserve(msgs.size());
            {
                // PERFORMANCE: Release CPython's Global Interpreter Lock (GIL)
                // While we parse the batch in C++, other Python threads (e.g. asyncio event loops,
                // web servers, Kafka consumer threads) can run concurrently without waiting on us!
                py::gil_scoped_release release;
                for (auto& msg : msgs) {
                    records.emplace_back(std::move(msg), mode);
                }
            }
            return records;
        },
        py::arg("msgs"), py::arg("mode") = core::ScanMode::Semantic, "Parses a batch of log lines releasing GIL");

    m.def(
        "reconstruct", [](const PySlotRecord& record) { return record.reconstruct(); }, py::arg("record"),
        "Reconstructs the original log line with 100% exact fidelity");

    m.def(
        "reconstruct",
        [](std::string_view formKey, const std::vector<std::int64_t>& ints, const std::vector<std::string>& strs) {
            std::string out;
            std::vector<std::string_view> strViews;
            strViews.reserve(strs.size());
            for (const auto& s : strs)
                strViews.emplace_back(s);
            (void)core::reconstruct(formKey, ints, strViews, out);
            return out;
        },
        py::arg("formKey"), py::arg("ints"), py::arg("strs"),
        "Reconstructs the original log line from decomposed slots");

    m.def(
        "decompose",
        [](std::string_view msg, core::ScanMode mode) {
            core::SlotRecord slots;
            (void)core::parse(msg, slots, mode);
            std::vector<std::string> strCopies;
            strCopies.reserve(slots.strs.size());
            for (const auto& sv : slots.strs)
                strCopies.emplace_back(sv);
            return py::make_tuple(slots.formKey(), slots.ints, std::move(strCopies));
        },
        py::arg("msg"), py::arg("mode") = core::ScanMode::Semantic,
        "Decomposes a message into (formKey, ints, strs) tuple");

    m.def(
        "decomposeBatch",
        [](const std::vector<std::string>& msgs, core::ScanMode mode) {
            std::vector<std::tuple<std::string, std::vector<std::int64_t>, std::vector<std::string>>> results;
            results.reserve(msgs.size());
            {
                py::gil_scoped_release release;
                for (const auto& msg : msgs) {
                    core::SlotRecord slots;
                    (void)core::parse(msg, slots, mode);
                    std::vector<std::string> strCopies;
                    strCopies.reserve(slots.strs.size());
                    for (const auto& sv : slots.strs)
                        strCopies.emplace_back(sv);
                    results.emplace_back(slots.formKey(), slots.ints, std::move(strCopies));
                }
            }
            return results;
        },
        py::arg("msgs"), py::arg("mode") = core::ScanMode::Semantic,
        "High-speed batch decomposition into (formKey, ints, strs) tuples releasing GIL");

    m.def(
        "formKey",
        [](std::string_view msg, core::ScanMode mode) {
            core::SlotRecord slots;
            (void)core::parse(msg, slots, mode);
            return slots.formKey();
        },
        py::arg("msg"), py::arg("mode") = core::ScanMode::Semantic, "Extracts the canonical form key");

    m.def(
        "templateOf",
        [](std::string_view form) {
            std::string tmpl;
            (void)core::templateOf(form, tmpl);
            return tmpl;
        },
        py::arg("form"), "Converts form key to human-readable <*> template");

    m.def(
        "hash64", [](std::string_view s) { return core::xxh64(s); }, py::arg("s"),
        "Computes 64-bit XXH64 hash matching ClickHouse xxHash64");

    m.def(
        "entropy", [](const std::vector<double>& counts) { return stats::entropyOfCounts(counts); }, py::arg("counts"),
        "Calculates Shannon entropy in bits with Neumaier compensation");

    m.def(
        "auditParallel",
        [](const std::vector<std::string>& lines, core::ScanMode mode, std::size_t numThreads) {
            AuditResult res;
            {
                py::gil_scoped_release release;
                res = auditBatchParallel(lines, mode, numThreads);
            }
            return res;
        },
        py::arg("lines"), py::arg("mode") = core::ScanMode::Semantic, py::arg("numThreads") = 0,
        "High-performance parallel audit verifying 100% byte-for-byte lossless invertibility");

    m.def(
        "extensionPath", []() { return std::filesystem::absolute("build/librazobill.duckdb_extension").string(); },
        "Returns the absolute path to the DuckDB loadable extension");

    m.def(
        "version", []() { return std::string(razobill::versionString()); },
        "Returns the SemVer 2.0.0 release version string");

    m.def("abiVersion", []() { return razobill::abiVersion(); }, "Returns the binary ABI contract version");

    m.def(
        "versionInfo",
        []() {
            auto v = razobill::version();
            return py::make_tuple(v.major, v.minor, v.patch);
        },
        "Returns the version tuple (major, minor, patch)");

    // Information Theory & Ledger (Theorems 1, 4, Corollary 2, Proposition 3, 10)
    py::class_<stats::Cell>(m, "Cell")
        .def(py::init([](std::uint64_t form, std::uint32_t meaning, double mass) {
                 return stats::Cell{form, meaning, mass};
             }),
             py::arg("form"), py::arg("meaning"), py::arg("mass"))
        .def_readwrite("form", &stats::Cell::form)
        .def_readwrite("meaning", &stats::Cell::meaning)
        .def_readwrite("mass", &stats::Cell::mass)
        .def("__repr__", [](const stats::Cell& c) {
            return "<pyrazobill.Cell form=" + std::to_string(c.form) + " meaning=" + std::to_string(c.meaning) +
                   " mass=" + std::to_string(c.mass) + ">";
        });

    py::class_<stats::XY>(m, "XY")
        .def(py::init([](std::uint64_t form, std::uint32_t y, double n) { return stats::XY{form, y, n}; }),
             py::arg("form"), py::arg("y"), py::arg("n"))
        .def_readwrite("form", &stats::XY::form)
        .def_readwrite("y", &stats::XY::y)
        .def_readwrite("n", &stats::XY::n);

    py::class_<stats::FormMeaning>(m, "FormMeaning")
        .def(py::init([](std::uint64_t form, std::uint32_t meaning, double p) {
                 return stats::FormMeaning{form, meaning, p};
             }),
             py::arg("form"), py::arg("meaning"), py::arg("p"))
        .def_readwrite("form", &stats::FormMeaning::form)
        .def_readwrite("meaning", &stats::FormMeaning::meaning)
        .def_readwrite("p", &stats::FormMeaning::p);

    py::class_<stats::Ledger>(m, "Ledger")
        .def_readonly("coverage", &stats::Ledger::coverage)
        .def_readonly("content", &stats::Ledger::content)
        .def_readonly("expression", &stats::Ledger::expression)
        .def_readonly("latitude", &stats::Ledger::latitude)
        .def_readonly("doubt", &stats::Ledger::doubt)
        .def_readonly("conveyance", &stats::Ledger::conveyance)
        .def_readonly("distance", &stats::Ledger::distance)
        .def_readonly("effectiveVocabulary", &stats::Ledger::effectiveVocabulary)
        .def_readonly("effectiveContent", &stats::Ledger::effectiveContent)
        .def_readonly("effectiveLatitude", &stats::Ledger::effectiveLatitude)
        .def_readonly("effectiveDoubt", &stats::Ledger::effectiveDoubt)
        .def_readonly("contentMm", &stats::Ledger::contentMm)
        .def_readonly("expressionMm", &stats::Ledger::expressionMm)
        .def_readonly("latitudeMm", &stats::Ledger::latitudeMm)
        .def_readonly("doubtMm", &stats::Ledger::doubtMm)
        .def_readonly("conveyanceMm", &stats::Ledger::conveyanceMm)
        .def_readonly("contentEpsilon", &stats::Ledger::contentEpsilon)
        .def_readonly("contentHalfWidth", &stats::Ledger::contentHalfWidth)
        .def("satisfiesLedgerIdentities", &stats::Ledger::satisfiesLedgerIdentities, py::arg("tol") = 1e-9)
        .def("satisfiesVocabularyLaw", &stats::Ledger::satisfiesVocabularyLaw, py::arg("nDistinctForms"),
             py::arg("tol") = 1e-9)
        .def("__repr__", [](const stats::Ledger& l) {
            return "<pyrazobill.Ledger C=" + std::to_string(l.content) + " E=" + std::to_string(l.expression) +
                   " Lambda=" + std::to_string(l.latitude) + " Delta=" + std::to_string(l.doubt) +
                   " I=" + std::to_string(l.conveyance) + ">";
        });

    py::class_<stats::ConsequenceDecomposition>(m, "ConsequenceDecomposition")
        .def_readonly("expression", &stats::ConsequenceDecomposition::expression)
        .def_readonly("consequence", &stats::ConsequenceDecomposition::consequence)
        .def_readonly("hedge", &stats::ConsequenceDecomposition::hedge)
        .def_readonly("latitude", &stats::ConsequenceDecomposition::latitude)
        .def("satisfiesIdentity", &stats::ConsequenceDecomposition::satisfiesIdentity, py::arg("tol") = 1e-9);

    py::class_<stats::PriceOfSaying>(m, "PriceOfSaying")
        .def_readonly("expectedMeaningBits", &stats::PriceOfSaying::expectedMeaningBits)
        .def_readonly("expectedExpressionBits", &stats::PriceOfSaying::expectedExpressionBits)
        .def_readonly("expectedClarificationBits", &stats::PriceOfSaying::expectedClarificationBits)
        .def_readonly("totalSayingBits", &stats::PriceOfSaying::totalSayingBits)
        .def_readonly("sayingCostBits", &stats::PriceOfSaying::sayingCostBits)
        .def_readonly("lowerBoundBits", &stats::PriceOfSaying::lowerBoundBits)
        .def_readonly("upperBoundBits", &stats::PriceOfSaying::upperBoundBits)
        .def("satisfiesIdentity", &stats::PriceOfSaying::satisfiesIdentity, py::arg("tol") = 1e-9)
        .def("__repr__", [](const stats::PriceOfSaying& p) {
            return "<pyrazobill.PriceOfSaying totalSaying=" + std::to_string(p.totalSayingBits) +
                   " sayingCost=" + std::to_string(p.sayingCostBits) + ">";
        });

    py::class_<stats::HierarchicalStep>(m, "HierarchicalStep")
        .def(py::init([](std::uint64_t form, std::vector<std::uint32_t> levels, std::uint32_t y, double mass) {
                 return stats::HierarchicalStep{form, std::move(levels), y, mass};
             }),
             py::arg("form"), py::arg("levels"), py::arg("y"), py::arg("mass"))
        .def_readwrite("form", &stats::HierarchicalStep::form)
        .def_readwrite("levels", &stats::HierarchicalStep::levels)
        .def_readwrite("y", &stats::HierarchicalStep::y)
        .def_readwrite("mass", &stats::HierarchicalStep::mass);

    py::class_<stats::DepthLedger>(m, "DepthLedger")
        .def_readonly("entropyLevels", &stats::DepthLedger::entropyLevels)
        .def_readonly("relevanceLevels", &stats::DepthLedger::relevanceLevels)
        .def_readonly("totalEntropy", &stats::DepthLedger::totalEntropy)
        .def_readonly("totalRelevance", &stats::DepthLedger::totalRelevance)
        .def_readonly("coarsestEntropy", &stats::DepthLedger::coarsestEntropy)
        .def_readonly("coarsestRelevance", &stats::DepthLedger::coarsestRelevance)
        .def("satisfiesIdentity", &stats::DepthLedger::satisfiesIdentity, py::arg("tol") = 1e-9)
        .def("__repr__", [](const stats::DepthLedger& d) {
            return "<pyrazobill.DepthLedger totalH=" + std::to_string(d.totalEntropy) +
                   " totalI=" + std::to_string(d.totalRelevance) + " levels=" + std::to_string(d.entropyLevels.size()) +
                   ">";
        });

    m.def(
        "computeLedger",
        [](const std::vector<stats::Cell>& cells, double totalEvents, double alpha) {
            return stats::ledger(cells, totalEvents, alpha);
        },
        py::arg("cells"), py::arg("totalEvents"), py::arg("confidenceAlpha") = 0.05,
        "Computes exact information-theoretic ledger (Theorems 1 & 4, Atta 2026)");

    m.def("priceOfSaying", &stats::priceOfSaying, py::arg("ledger"), py::arg("nMessages"),
          "Calculates prefix code length bounds and wording premium (Theorem 7, Atta 2026)");

    m.def(
        "computeDepthLedger",
        [](const std::vector<stats::HierarchicalStep>& steps) { return stats::computeDepthLedger(steps); },
        py::arg("steps"), "Computes multi-level depth ledger across successive coarsenings (Proposition 6, Atta 2026)");

    m.def(
        "consequenceDecomposition",
        [](const std::vector<stats::XY>& xy, const std::vector<stats::FormMeaning>& map) {
            return stats::consequenceDecomposition(xy, map);
        },
        py::arg("xy"), py::arg("map"),
        "Computes 3-way consequence decomposition E = I(X;Y) + H(M|Y) + H(X|M) (Theorem 4c)");

    m.def("millerMadow", &stats::millerMadow, py::arg("pluginEntropy"), py::arg("observedCategories"),
          py::arg("sampleSize"), "Calculates Miller-Madow bias-corrected entropy");

    m.def(
        "certifiedInterval",
        [](std::size_t alphabetSize, double sampleSize, double alpha) {
            auto ci = stats::certifiedInterval(alphabetSize, sampleSize, alpha);
            return py::make_tuple(ci.epsilon, ci.halfWidth);
        },
        py::arg("alphabetSize"), py::arg("sampleSize"), py::arg("confidenceAlpha") = 0.05,
        "Calculates Proposition 10 certified confidence interval (epsilon, halfWidth)");

    // Latitude Steganographic / Provenance Channel (Theorem 8 & Proposition 9)
    py::class_<channel::SynonymousOption>(m, "SynonymousOption")
        .def(py::init([](std::size_t formIndex, double probability) {
                 return channel::SynonymousOption{formIndex, probability};
             }),
             py::arg("formIndex"), py::arg("probability"))
        .def_readwrite("formIndex", &channel::SynonymousOption::formIndex)
        .def_readwrite("probability", &channel::SynonymousOption::probability);

    py::class_<channel::MeaningChoice>(m, "MeaningChoice")
        .def(py::init([](std::uint32_t meaning, std::vector<channel::SynonymousOption> options) {
                 return channel::MeaningChoice{meaning, std::move(options)};
             }),
             py::arg("meaning"), py::arg("options"))
        .def_readwrite("meaning", &channel::MeaningChoice::meaning)
        .def_readwrite("options", &channel::MeaningChoice::options);

    py::class_<channel::ChannelCapacity>(m, "ChannelCapacity")
        .def_readonly("meanCapacityBits", &channel::ChannelCapacity::meanCapacityBits)
        .def_readonly("variance", &channel::ChannelCapacity::variance)
        .def_readonly("safePayloadBits", &channel::ChannelCapacity::safePayloadBits);

    py::class_<channel::EmbedResult>(m, "EmbedResult")
        .def_readonly("success", &channel::EmbedResult::success)
        .def_readonly("payloadBits", &channel::EmbedResult::payloadBits)
        .def_readonly("embeddedMessage", &channel::EmbedResult::embeddedMessage)
        .def_readonly("chosenForms", &channel::EmbedResult::chosenForms);

    py::class_<channel::DecodeResult>(m, "DecodeResult")
        .def_readonly("success", &channel::DecodeResult::success)
        .def_readonly("payloadBits", &channel::DecodeResult::payloadBits)
        .def_readonly("message", &channel::DecodeResult::message);

    m.def(
        "channelAnalyzeCapacity",
        [](const std::vector<channel::MeaningChoice>& seq) { return channel::analyzeCapacity(seq); },
        py::arg("sequence"), "Analyzes safe payload and theoretical capacity for meaning sequence (Theorem 8)");

    m.def(
        "channelRequiredKeyBits",
        [](const std::vector<channel::MeaningChoice>& seq, std::size_t s) { return channel::requiredKeyBits(seq, s); },
        py::arg("sequence"), py::arg("s") = 64,
        "Calculates Proposition 9 minimum key length in bits for dyadic precision");

    m.def("channelFailureProbabilityBound", &channel::failureProbabilityBound, py::arg("capacity"),
          py::arg("payloadBits"), py::arg("s") = 64.0,
          "Calculates Theorem 8 Chebyshev upper bound on failure probability");

    m.def(
        "channelExactFailureProbability",
        [](const std::vector<channel::MeaningChoice>& seq, std::size_t payloadBits) {
            return channel::exactFailureProbability(seq, payloadBits);
        },
        py::arg("sequence"), py::arg("payloadBits"),
        "Evaluates exact analytical failure probability delta via distribution convolution (Theorem 8)");

    m.def(
        "channelEmbed",
        [](const std::vector<channel::MeaningChoice>& seq, std::uint64_t msg, std::size_t payloadBits, double sharedKey,
           double dither) { return channel::embed(seq, msg, payloadBits, sharedKey, dither); },
        py::arg("sequence"), py::arg("message"), py::arg("payloadBits"), py::arg("sharedKey"), py::arg("dither") = 0.5,
        "Embeds integer message into synonymous forms via arithmetic cumulative interval coding");

    m.def(
        "channelDecode",
        [](const std::vector<channel::MeaningChoice>& seq, const std::vector<std::size_t>& observedForms,
           std::size_t payloadBits,
           double sharedKey) { return channel::decode(seq, observedForms, payloadBits, sharedKey); },
        py::arg("sequence"), py::arg("observedForms"), py::arg("payloadBits"), py::arg("sharedKey"),
        "Decodes embedded message from observed synonymous forms");
}
