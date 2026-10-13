// Universal C ABI verification suite: validates that foreign language FFI callers
// (Rust, Go, Zig, C#, Python) can safely allocate, parse, and reconstruct without C++ runtime leakage.
#include <cstring>
#include <string>

#include "doctest.h"
#include "librazobill/RazobillC.h"

TEST_CASE("Universal C ABI: RzParse and RzReconstruct") {
    // Asserting caller-owned buffer safety:
    // The C caller provides a raw stack buffer; C++ writes directly into it without heap copies.
    const char* msg = "Failed password for root from 192.168.1.1 port 22";
    size_t len = std::strlen(msg);

    RzSlotRecord record{};
    RzStatusCode status = RzParse(msg, len, RZ_SCAN_SEMANTIC, &record);
    REQUIRE(status == RZ_SUCCESS);
    CHECK(record.formKey != nullptr);
    CHECK(record.formKeyLength > 0);
    CHECK(record.formKeyHash != 0);
    CHECK(record.intCount == 1);
    CHECK(record.intValues[0] == 22);
    CHECK(record.strCount == 2);

    char outBuf[256];
    size_t outLen = 0;
    status = RzReconstruct(&record, outBuf, sizeof(outBuf), &outLen);
    REQUIRE(status == RZ_SUCCESS);
    CHECK(outLen == len);
    CHECK(std::string(outBuf, outLen) == msg);

    char tmplBuf[256];
    size_t tmplLen = 0;
    status = RzTemplateOf(record.formKey, record.formKeyLength, tmplBuf, sizeof(tmplBuf), &tmplLen);
    REQUIRE(status == RZ_SUCCESS);
    CHECK(std::string(tmplBuf, tmplLen) == "Failed password for <*> from <*> port <*>");

    // Symmetrical slot-based reconstruction
    char slotOutBuf[256];
    size_t slotOutLen = 0;
    status =
        RzReconstructSlots(record.formKey, record.formKeyLength, record.intValues, record.intCount, record.strValues,
                           record.strLengths, record.strCount, slotOutBuf, sizeof(slotOutBuf), &slotOutLen);
    REQUIRE(status == RZ_SUCCESS);
    CHECK(slotOutLen == len);
    CHECK(std::string(slotOutBuf, slotOutLen) == msg);

    RzFreeRecord(&record);
}

TEST_CASE("Universal C ABI: RzDecompose") {
    const char* msg = "Accepted publickey for ubuntu from 10.0.0.1 port 54322 ssh2";
    size_t len = std::strlen(msg);

    RzSlotRecord record{};
    RzStatusCode status = RzDecompose(msg, len, RZ_SCAN_SEMANTIC, &record);
    REQUIRE(status == RZ_SUCCESS);
    CHECK(record.intCount == 1);
    CHECK(record.intValues[0] == 54322);

    char outBuf[256];
    size_t outLen = 0;
    status = RzReconstructSlots(record.formKey, record.formKeyLength, record.intValues, record.intCount,
                                record.strValues, record.strLengths, record.strCount, outBuf, sizeof(outBuf), &outLen);
    REQUIRE(status == RZ_SUCCESS);
    CHECK(std::string(outBuf, outLen) == msg);

    RzFreeRecord(&record);
}

TEST_CASE("Universal C ABI: Hash and Version") {
    CHECK(RzHash64("", 0) == 0xEF46DB3751D8E999ULL);
    CHECK(std::string(RzVersion()) == "1.0.1");
    CHECK(RzVersionMajor() == 1);
    CHECK(RzVersionMinor() == 0);
    CHECK(RzVersionPatch() == 1);
    CHECK(RzAbiVersion() == 1);
}

TEST_CASE("Universal C ABI: Information Theory and Ledger computation") {
    // Figure 1 cells passed via pure C ABI struct array
    const RzCell cells[] = {{0, 0, 0.25}, {1, 0, 0.25}, {1, 1, 0.125}, {1, 2, 0.25}, {2, 2, 0.125}};
    RzLedger ledger{};
    RzStatusCode status = RzComputeLedger(cells, 5, 1.0, 0.05, &ledger);
    REQUIRE(status == RZ_SUCCESS);
    CHECK(ledger.coverage == doctest::Approx(1.0));
    CHECK(ledger.content == doctest::Approx(1.406).epsilon(1e-3));
    CHECK(ledger.expression == doctest::Approx(1.299).epsilon(1e-3));
    CHECK(ledger.latitude == doctest::Approx(0.844).epsilon(1e-3));
    CHECK(ledger.doubt == doctest::Approx(0.951).epsilon(1e-3));
    CHECK(ledger.conveyance == doctest::Approx(0.454).epsilon(2e-3));
    CHECK(ledger.distance == doctest::Approx(ledger.latitude + ledger.doubt).epsilon(1e-9));
    CHECK(ledger.effectiveVocabulary > 0.0);
    CHECK(ledger.effectiveContent > 0.0);
    CHECK(ledger.contentMm >= ledger.content);

    // Invalid parameters guard
    CHECK(RzComputeLedger(nullptr, 5, 1000.0, 0.05, &ledger) == RZ_ERR_INVALID_PARAM);
    CHECK(RzComputeLedger(cells, 0, 1000.0, 0.05, &ledger) == RZ_ERR_INVALID_PARAM);
    CHECK(RzComputeLedger(cells, 5, -1.0, 0.05, &ledger) == RZ_ERR_INVALID_PARAM);

    // Miller-Madow correction
    double corrected = RzMillerMadow(1.5, 4, 1000.0);
    CHECK(corrected > 1.5);

    // Certified interval
    RzCertifiedInterval ci = RzComputeCertifiedInterval(25, 10000.0, 0.05);
    CHECK(ci.epsilon > 0.0);
    CHECK(ci.halfWidth > 0.0);

    // Theorem 7: Price of Saying via C ABI
    RzPriceOfSaying price{};
    REQUIRE(RzComputePriceOfSaying(&ledger, 100.0, &price) == RZ_SUCCESS);
    CHECK(price.expectedMeaningBits == doctest::Approx(100.0 * ledger.content));
    CHECK(price.expectedExpressionBits == doctest::Approx(100.0 * ledger.expression));
    CHECK(price.sayingCostBits == doctest::Approx(100.0 * ledger.latitude));
    CHECK(price.totalSayingBits == doctest::Approx(price.expectedMeaningBits + price.sayingCostBits));
    CHECK(price.lowerBoundBits == price.totalSayingBits);
    CHECK(price.upperBoundBits == price.totalSayingBits + 2.0);
    CHECK(RzComputePriceOfSaying(nullptr, 100.0, &price) == RZ_ERR_INVALID_PARAM);
    CHECK(RzComputePriceOfSaying(&ledger, -1.0, &price) == RZ_ERR_INVALID_PARAM);
}

TEST_CASE("Universal C ABI: RzFreeRecord safe pointer zeroing and idempotency") {
    const char* msg = "User alice logged in from 10.0.0.1";
    RzSlotRecord record{};
    RzStatusCode status = RzParse(msg, std::strlen(msg), RZ_SCAN_SEMANTIC, &record);
    REQUIRE(status == RZ_SUCCESS);
    CHECK(record.internalHandle != nullptr);
    CHECK(record.formKey != nullptr);
    CHECK(record.strValues != nullptr);

    // Free record and verify all dangling pointers are zeroed
    RzFreeRecord(&record);
    CHECK(record.internalHandle == nullptr);
    CHECK(record.formKey == nullptr);
    CHECK(record.formKeyLength == 0);
    CHECK(record.formKeyHash == 0);
    CHECK(record.intValues == nullptr);
    CHECK(record.intCount == 0);
    CHECK(record.strValues == nullptr);
    CHECK(record.strLengths == nullptr);
    CHECK(record.strCount == 0);

    // Idempotent free must not crash
    RzFreeRecord(&record);
    RzFreeRecord(nullptr);
}

TEST_CASE("Universal C ABI: Exception barrier and error code mappings") {
    CHECK(RZ_ERR_NO_MEMORY == 6);
    CHECK(RZ_ERR_INTERNAL == 7);

    // Invalid parameters guard across entrypoints
    CHECK(RzParse(nullptr, 10, RZ_SCAN_EXACT, nullptr) == RZ_ERR_INVALID_PARAM);
    RzSlotRecord rec{};
    CHECK(RzParse(nullptr, 10, RZ_SCAN_EXACT, &rec) == RZ_ERR_INVALID_PARAM);
    const char* m = "test";
    CHECK(RzParse(m, 4, RZ_SCAN_EXACT, nullptr) == RZ_ERR_INVALID_PARAM);

    char buf[64];
    size_t outLen = 0;
    CHECK(RzReconstruct(nullptr, buf, sizeof(buf), &outLen) == RZ_ERR_INVALID_PARAM);
    CHECK(RzReconstructSlots(nullptr, 0, nullptr, 0, nullptr, nullptr, 0, buf, sizeof(buf), &outLen) ==
          RZ_ERR_INVALID_PARAM);
    CHECK(RzTemplateOf(nullptr, 0, buf, sizeof(buf), &outLen) == RZ_ERR_INVALID_PARAM);
}

