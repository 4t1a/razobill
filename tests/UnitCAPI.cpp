// Universal C ABI verification suite for Razobill v2.0
#include <cstring>
#include <string>
#include <vector>

#include "doctest.h"
#include "razobill.h"

TEST_CASE("Universal C ABI: razobill_split and razobill_join roundtrip") {
    const char* msg = "Failed password for root from 192.168.1.1 port 22";
    const size_t len = std::strlen(msg);

    razobill_record_t record{};
    razobill_status_t status = razobill_split(msg, len, &record);
    REQUIRE(status == RAZOBILL_OK);
    CHECK(record.template_str != nullptr);
    CHECK(record.template_len > 0);
    CHECK(record.int_count == 1);
    CHECK(record.ints[0] == 22);
    CHECK(record.str_count == 2);

    // Verify template content
    std::string tmpl(record.template_str, record.template_len);
    CHECK(tmpl == "Failed password for {s} from {s} port {i}");

    // Buffer capacity probing
    size_t requiredLen = 0;
    status = razobill_join(&record, nullptr, 0, &requiredLen);
    REQUIRE(status == RAZOBILL_ERR_BUFFER_TOO_SMALL);
    CHECK(requiredLen == len);

    // Exact buffer fill: buffer is exactly `len` bytes, no space for trailing \0
    std::vector<char> outBuf(len, 'X');
    size_t outLen = 0;
    status = razobill_join(&record, outBuf.data(), outBuf.size(), &outLen);
    REQUIRE(status == RAZOBILL_OK);
    CHECK(outLen == len);
    CHECK(std::string(outBuf.data(), outLen) == msg);

    // Template extraction function
    std::vector<char> tmplBuf(tmpl.size());
    size_t tmplLen = 0;
    status = razobill_template(msg, len, tmplBuf.data(), tmplBuf.size(), &tmplLen);
    REQUIRE(status == RAZOBILL_OK);
    CHECK(tmplLen == tmpl.size());
    CHECK(std::string(tmplBuf.data(), tmplLen) == tmpl);

    razobill_free(&record);
    CHECK(record.template_str == nullptr);
    CHECK(record.internal_handle == nullptr);
}

TEST_CASE("Universal C ABI: Literal brace escaping in templates") {
    const char* msg = "Payload {action: login} port=443 {status}";
    const size_t len = std::strlen(msg);

    razobill_record_t record{};
    razobill_status_t status = razobill_split(msg, len, &record);
    REQUIRE(status == RAZOBILL_OK);

    std::string tmpl(record.template_str, record.template_len);
    // {action: login} -> {action: {s}} inside {{ ... }}
    CHECK(tmpl.find("{{") != std::string::npos);
    CHECK(tmpl.find("}}") != std::string::npos);

    // Full roundtrip byte-exact fidelity
    std::vector<char> reconstructed(len);
    size_t recLen = 0;
    status = razobill_join(&record, reconstructed.data(), reconstructed.size(), &recLen);
    REQUIRE(status == RAZOBILL_OK);
    CHECK(recLen == len);
    CHECK(std::string(reconstructed.data(), recLen) == msg);

    razobill_free(&record);
}

TEST_CASE("Universal C ABI: FFI Caller manually populating record for join") {
    // Foreign caller constructs record from scratch without calling razobill_split
    const char* tmpl = "System status: {s}, code={i}";
    const int64_t ints[] = {404};
    const char* strVal = "NOT_FOUND";
    const size_t strLen = std::strlen(strVal);
    const char* const strs[] = {strVal};
    const size_t strLens[] = {strLen};

    razobill_record_t record{};
    record.template_str = tmpl;
    record.template_len = std::strlen(tmpl);
    record.ints = ints;
    record.int_count = 1;
    record.strs = strs;
    record.str_lens = strLens;
    record.str_count = 1;
    record.internal_handle = nullptr; // Manual FFI record

    char outBuf[128];
    size_t outLen = 0;
    razobill_status_t status = razobill_join(&record, outBuf, sizeof(outBuf), &outLen);
    REQUIRE(status == RAZOBILL_OK);
    CHECK(std::string(outBuf, outLen) == "System status: NOT_FOUND, code=404");

    // razobill_free is safe on manually constructed / non-allocated records
    razobill_free(&record);
}

TEST_CASE("Universal C ABI: Edge cases, empty string, errors and metadata") {
    // Empty message
    razobill_record_t emptyRecord{};
    razobill_status_t status = razobill_split("", 0, &emptyRecord);
    REQUIRE(status == RAZOBILL_OK);
    CHECK(emptyRecord.template_len == 0);
    CHECK(emptyRecord.int_count == 0);
    CHECK(emptyRecord.str_count == 0);

    char emptyBuf[8];
    size_t emptyLen = 99;
    status = razobill_join(&emptyRecord, emptyBuf, sizeof(emptyBuf), &emptyLen);
    REQUIRE(status == RAZOBILL_OK);
    CHECK(emptyLen == 0);
    razobill_free(&emptyRecord);

    // Null argument handling
    CHECK(razobill_split(nullptr, 10, &emptyRecord) == RAZOBILL_ERR_INVALID_ARGUMENT);
    CHECK(razobill_split("abc", 3, nullptr) == RAZOBILL_ERR_INVALID_ARGUMENT);
    CHECK(razobill_join(nullptr, emptyBuf, sizeof(emptyBuf), &emptyLen) == RAZOBILL_ERR_INVALID_ARGUMENT);

    // Status messages
    CHECK(std::string(razobill_status_message(RAZOBILL_OK)) == "Success");
    CHECK(std::string(razobill_status_message(RAZOBILL_ERR_BUFFER_TOO_SMALL)).find("insufficient") != std::string::npos);

    // Version
    CHECK(std::string(razobill_version()) == "2.0.0");
}
