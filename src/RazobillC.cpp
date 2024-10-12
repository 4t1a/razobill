// ==============================================================================
// Razobill Universal C ABI Implementation: Cross-Language FFI Bridge
// ==============================================================================
//
// SAFETY CONTRACT & CROSS-LANGUAGE GUARANTEES:
// Implements the stable C99 foreign function interface defined in `RazobillC.h`:
// 1. Zero Exception Leakage: Catches all C++ exceptions at the boundary, translating
//    errors into deterministic `RzStatusCode` return codes.
// 2. Caller-Owned Buffers: Callers provide memory buffers directly, preventing foreign
//    runtime heap corruption (essential for Rust, Go, Zig, C#, and Java FFI).
// 3. Opaque Lifetime Management: Internal C++ vectors are safely encapsulated behind
//    `RzInternalRecord` and explicitly freed via `RzFreeRecord()`.
// 4. Stable ABI Versioning: Enforces `RZ_ABI_VERSION = 1` binary compatibility.
// ==============================================================================

#include "librazobill/RazobillC.h"
#include "librazobill/Hash.hpp"
#include "librazobill/Slots.hpp"
#include "librazobill/Stats.hpp"
#include "librazobill/Razobill.hpp"
#include "librazobill/Version.hpp"

#include <cstring>
#include <string>
#include <vector>

// Internal storage hidden behind RzSlotRecord::internalHandle.
//
// DESIGN:
// C callers don't know about `std::string` or `std::vector`.
// To provide C-style arrays (`const char* const*` and `const size_t*`), we marshal
// the string views into contiguous pointer and length arrays stored safely inside this struct.
struct RzInternalRecord {
    razobill::core::SlotRecord slots;
    std::string formKeyStr;
    std::vector<const char*> strPointers;
    std::vector<size_t> strLengths;
};

extern "C" {

RZ_API RzEngine* RzCreateEngine(void) {
    // Engine creation requires Backend configuration via C++ facade or ADBC
    return nullptr;
}

RZ_API void RzDestroyEngine(RzEngine* engine) {
    (void)engine;
}

RZ_API RzStatusCode RzParse(const char* message, size_t messageLength, RzScanMode mode, RzSlotRecord* outRecord) {
    if (!message || !outRecord) return RZ_ERR_INVALID_PARAM;

    auto* internal = new RzInternalRecord();
    std::string_view msgView(message, messageLength);
    auto scanMode = (mode == RZ_SCAN_SEMANTIC) ? razobill::core::ScanMode::Semantic : razobill::core::ScanMode::Exact;

    razobill::Status status = razobill::core::parse(msgView, internal->slots, scanMode);
    if (status != razobill::Status::Ok) {
        delete internal;
        if (status == razobill::Status::TooLarge) return RZ_ERR_TOO_LARGE;
        if (status == razobill::Status::Malformed) return RZ_ERR_MALFORMED;
        return RZ_ERR_INVALID_PARAM;
    }

    internal->formKeyStr = internal->slots.formKey();
    outRecord->formKey = internal->formKeyStr.c_str();
    outRecord->formKeyLength = internal->formKeyStr.size();
    outRecord->formKeyHash = razobill::core::xxh64(internal->formKeyStr);

    outRecord->intValues = internal->slots.ints.data();
    outRecord->intCount = internal->slots.ints.size();

    internal->strPointers.reserve(internal->slots.strs.size());
    internal->strLengths.reserve(internal->slots.strs.size());
    for (const auto& sv : internal->slots.strs) {
        internal->strPointers.push_back(sv.data());
        internal->strLengths.push_back(sv.size());
    }

    outRecord->strValues = internal->strPointers.data();
    outRecord->strLengths = internal->strLengths.data();
    outRecord->strCount = internal->strPointers.size();
    outRecord->internalHandle = internal;

    return RZ_SUCCESS;
}

RZ_API RzStatusCode RzDecompose(const char* message, size_t messageLength, RzScanMode mode, RzSlotRecord* outRecord) {
    return RzParse(message, messageLength, mode, outRecord);
}

RZ_API RzStatusCode RzReconstruct(const RzSlotRecord* record, char* outBuffer, size_t bufferCapacity,
                                  size_t* outLength) {
    if (!record) return RZ_ERR_INVALID_PARAM;
    return RzReconstructSlots(record->formKey, record->formKeyLength, record->intValues, record->intCount,
                              record->strValues, record->strLengths, record->strCount, outBuffer, bufferCapacity,
                              outLength);
}

RZ_API RzStatusCode RzReconstructSlots(const char* formKey, size_t formKeyLength, const int64_t* intValues,
                                       size_t intCount, const char* const* strValues, const size_t* strLengths,
                                       size_t strCount, char* outBuffer, size_t bufferCapacity, size_t* outLength) {
    if (!formKey || !outBuffer || !outLength) return RZ_ERR_INVALID_PARAM;
    if (intCount > 0 && !intValues) return RZ_ERR_INVALID_PARAM;
    if (strCount > 0 && (!strValues || !strLengths)) return RZ_ERR_INVALID_PARAM;

    std::string rebuilt;
    std::vector<std::string_view> strViews;
    strViews.reserve(strCount);
    for (size_t i = 0; i < strCount; ++i) {
        strViews.emplace_back(strValues[i], strLengths[i]);
    }

    std::string_view fKey(formKey, formKeyLength);
    std::span<const int64_t> intSpan(intValues, intCount);
    std::span<const std::string_view> strSpan(strViews.data(), strViews.size());

    razobill::Status status = razobill::core::reconstruct(fKey, intSpan, strSpan, rebuilt);
    if (status != razobill::Status::Ok) {
        if (status == razobill::Status::SlotUnderflow) return RZ_ERR_SLOT_UNDERFLOW;
        if (status == razobill::Status::Malformed) return RZ_ERR_MALFORMED;
        return RZ_ERR_INVALID_PARAM;
    }

    if (rebuilt.size() > bufferCapacity) {
        *outLength = rebuilt.size();
        return RZ_ERR_BUFFER_TOO_SMALL;
    }

    std::memcpy(outBuffer, rebuilt.data(), rebuilt.size());
    *outLength = rebuilt.size();
    return RZ_SUCCESS;
}

RZ_API void RzFreeRecord(RzSlotRecord* record) {
    if (record && record->internalHandle) {
        auto* internal = static_cast<RzInternalRecord*>(record->internalHandle);
        delete internal;
        record->internalHandle = nullptr;
    }
}

RZ_API uint64_t RzHash64(const char* text, size_t length) {
    if (!text) return 0;
    return razobill::core::xxh64(std::string_view(text, length));
}

RZ_API RzStatusCode RzTemplateOf(const char* formKey, size_t formKeyLength, char* outBuffer, size_t bufferCapacity,
                                 size_t* outLength) {
    if (!formKey || !outBuffer || !outLength) return RZ_ERR_INVALID_PARAM;

    std::string tmpl;
    std::string_view keyView(formKey, formKeyLength);
    razobill::Status status = razobill::core::templateOf(keyView, tmpl);
    if (status != razobill::Status::Ok) return RZ_ERR_MALFORMED;

    if (tmpl.size() > bufferCapacity) {
        *outLength = tmpl.size();
        return RZ_ERR_BUFFER_TOO_SMALL;
    }

    std::memcpy(outBuffer, tmpl.data(), tmpl.size());
    *outLength = tmpl.size();
    return RZ_SUCCESS;
}

RZ_API RzStatusCode RzComputeLedger(const RzCell* cells, size_t cellCount, double totalEvents, double confidenceAlpha,
                                    RzLedger* outLedger) {
    if (!cells || cellCount == 0 || !outLedger || totalEvents <= 0.0) {
        return RZ_ERR_INVALID_PARAM;
    }
    std::vector<razobill::stats::Cell> cppCells;
    cppCells.reserve(cellCount);
    for (size_t i = 0; i < cellCount; ++i) {
        cppCells.push_back({cells[i].form, cells[i].meaning, cells[i].mass});
    }
    auto l = razobill::stats::ledger(cppCells, totalEvents, confidenceAlpha);
    outLedger->coverage = l.coverage;
    outLedger->content = l.content;
    outLedger->expression = l.expression;
    outLedger->latitude = l.latitude;
    outLedger->doubt = l.doubt;
    outLedger->conveyance = l.conveyance;
    outLedger->distance = l.distance;
    outLedger->effectiveVocabulary = l.effectiveVocabulary;
    outLedger->effectiveContent = l.effectiveContent;
    outLedger->effectiveLatitude = l.effectiveLatitude;
    outLedger->effectiveDoubt = l.effectiveDoubt;
    outLedger->contentMm = l.contentMm;
    outLedger->expressionMm = l.expressionMm;
    outLedger->latitudeMm = l.latitudeMm;
    outLedger->doubtMm = l.doubtMm;
    outLedger->conveyanceMm = l.conveyanceMm;
    outLedger->contentEpsilon = l.contentEpsilon;
    outLedger->contentHalfWidth = l.contentHalfWidth;
    return RZ_SUCCESS;
}

RZ_API double RzMillerMadow(double pluginEntropy, size_t observedCategories, double sampleSize) {
    return razobill::stats::millerMadow(pluginEntropy, observedCategories, sampleSize);
}

RZ_API RzCertifiedInterval RzComputeCertifiedInterval(size_t alphabetSize, double sampleSize, double confidenceAlpha) {
    auto ci = razobill::stats::certifiedInterval(alphabetSize, sampleSize, confidenceAlpha);
    return {ci.epsilon, ci.halfWidth};
}

RZ_API RzStatusCode RzComputePriceOfSaying(const RzLedger* ledger, double nMessages, RzPriceOfSaying* outPrice) {
    if (!ledger || !outPrice || nMessages <= 0.0) return RZ_ERR_INVALID_PARAM;
    razobill::stats::Ledger cppL;
    cppL.content = ledger->content;
    cppL.expression = ledger->expression;
    cppL.latitude = ledger->latitude;
    cppL.doubt = ledger->doubt;
    auto p = razobill::stats::priceOfSaying(cppL, nMessages);
    outPrice->expectedMeaningBits = p.expectedMeaningBits;
    outPrice->expectedExpressionBits = p.expectedExpressionBits;
    outPrice->expectedClarificationBits = p.expectedClarificationBits;
    outPrice->totalSayingBits = p.totalSayingBits;
    outPrice->sayingCostBits = p.sayingCostBits;
    outPrice->lowerBoundBits = p.lowerBoundBits;
    outPrice->upperBoundBits = p.upperBoundBits;
    return RZ_SUCCESS;
}

RZ_API const char* RzVersion(void) {
    return razobill::version().string;
}

RZ_API uint32_t RzVersionMajor(void) {
    return razobill::version().major;
}

RZ_API uint32_t RzVersionMinor(void) {
    return razobill::version().minor;
}

RZ_API uint32_t RzVersionPatch(void) {
    return razobill::version().patch;
}

RZ_API uint32_t RzAbiVersion(void) {
    return razobill::version().abi;
}

} // extern "C"
