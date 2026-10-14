// ==============================================================================
// Razobill Universal C ABI Implementation (Unified CamelCase Rz API)
// ==============================================================================

#include "librazobill/RazobillC.h"
#include "librazobill/Slots.hpp"
#include "librazobill/Version.hpp"

#include <cstring>
#include <exception>
#include <memory>
#include <new>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct InternalRecord {
    razobill::core::SlotRecord slots;
    std::vector<const char*> strPointers;
    std::vector<size_t> strLengths;
};

} // namespace

extern "C" {

RZ_API RzStatus RzSplit(const char* message, size_t messageLength, RzRecord* outRecord) {
    if (!outRecord) return RZ_ERR_INVALID_ARGUMENT;
    if (!message && messageLength > 0) return RZ_ERR_INVALID_ARGUMENT;

    std::memset(outRecord, 0, sizeof(RzRecord));

    if (messageLength == 0) {
        static const char emptyStr = '\0';
        outRecord->templateStr = &emptyStr;
        outRecord->templateLength = 0;
        return RZ_OK;
    }

    try {
        auto internal = std::make_unique<InternalRecord>();
        const std::string_view msgView(message, messageLength);

        const razobill::Status status =
            razobill::core::parse(msgView, internal->slots, razobill::core::ScanMode::Semantic);
        if (status != razobill::Status::Ok) {
            if (status == razobill::Status::TooLarge) return RZ_ERR_MESSAGE_TOO_LARGE;
            if (status == razobill::Status::Malformed) return RZ_ERR_MALFORMED_TEMPLATE;
            return RZ_ERR_INVALID_ARGUMENT;
        }

        const auto& strs = internal->slots.strs;
        internal->strPointers.reserve(strs.size());
        internal->strLengths.reserve(strs.size());
        for (const auto& sv : strs) {
            internal->strPointers.push_back(sv.data());
            internal->strLengths.push_back(sv.size());
        }

        outRecord->templateStr = internal->slots.templateStr.data();
        outRecord->templateLength = internal->slots.templateStr.size();
        outRecord->intValues = internal->slots.ints.data();
        outRecord->intCount = internal->slots.ints.size();
        outRecord->strValues = internal->strPointers.data();
        outRecord->strLengths = internal->strLengths.data();
        outRecord->strCount = internal->strPointers.size();
        outRecord->internalHandle = internal.release();

        return RZ_OK;
    } catch (const std::bad_alloc&) {
        return RZ_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return RZ_ERR_INTERNAL;
    }
}

RZ_API RzStatus RzJoin(const RzRecord* record, char* outBuffer, size_t bufferCapacity, size_t* outLength) {
    if (!record) return RZ_ERR_INVALID_ARGUMENT;
    if (!record->templateStr && record->templateLength > 0) return RZ_ERR_INVALID_ARGUMENT;

    try {
        const std::string_view tmplView(record->templateStr ? record->templateStr : "", record->templateLength);
        std::span<const int64_t> intSpan(record->intValues ? record->intValues : nullptr, record->intCount);

        std::vector<std::string_view> strViews;
        strViews.reserve(record->strCount);
        for (size_t i = 0; i < record->strCount; ++i) {
            const char* sPtr = record->strValues ? record->strValues[i] : nullptr;
            const size_t sLen = record->strLengths ? record->strLengths[i] : 0;
            if (!sPtr && sLen > 0) return RZ_ERR_INVALID_ARGUMENT;
            strViews.emplace_back(sPtr ? sPtr : "", sLen);
        }

        std::string result;
        const razobill::Status status = razobill::core::reconstruct(tmplView, intSpan, strViews, result);
        if (status != razobill::Status::Ok) {
            if (status == razobill::Status::SlotUnderflow) return RZ_ERR_COUNT_MISMATCH;
            if (status == razobill::Status::Malformed) return RZ_ERR_MALFORMED_TEMPLATE;
            if (status == razobill::Status::TooLarge) return RZ_ERR_MESSAGE_TOO_LARGE;
            return RZ_ERR_INVALID_ARGUMENT;
        }

        const size_t reqLen = result.size();
        if (outLength) *outLength = reqLen;

        if (!outBuffer || bufferCapacity < reqLen) {
            return RZ_ERR_BUFFER_TOO_SMALL;
        }

        if (reqLen > 0) {
            std::memcpy(outBuffer, result.data(), reqLen);
        }
        return RZ_OK;
    } catch (const std::bad_alloc&) {
        return RZ_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return RZ_ERR_INTERNAL;
    }
}

RZ_API RzStatus RzTemplate(const char* message, size_t messageLength, char* outBuffer, size_t bufferCapacity,
                           size_t* outLength) {
    if (!message && messageLength > 0) return RZ_ERR_INVALID_ARGUMENT;

    try {
        if (messageLength == 0) {
            if (outLength) *outLength = 0;
            return RZ_OK;
        }

        razobill::core::SlotRecord slots;
        const std::string_view msgView(message, messageLength);
        const razobill::Status status = razobill::core::parse(msgView, slots, razobill::core::ScanMode::Semantic);
        if (status != razobill::Status::Ok) {
            if (status == razobill::Status::TooLarge) return RZ_ERR_MESSAGE_TOO_LARGE;
            if (status == razobill::Status::Malformed) return RZ_ERR_MALFORMED_TEMPLATE;
            return RZ_ERR_INVALID_ARGUMENT;
        }

        const size_t reqLen = slots.templateStr.size();
        if (outLength) *outLength = reqLen;

        if (!outBuffer || bufferCapacity < reqLen) {
            return RZ_ERR_BUFFER_TOO_SMALL;
        }

        if (reqLen > 0) {
            std::memcpy(outBuffer, slots.templateStr.data(), reqLen);
        }
        return RZ_OK;
    } catch (const std::bad_alloc&) {
        return RZ_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return RZ_ERR_INTERNAL;
    }
}

RZ_API void RzFree(RzRecord* record) {
    if (!record) return;
    if (record->internalHandle) {
        delete static_cast<InternalRecord*>(record->internalHandle);
        record->internalHandle = nullptr;
    }
    std::memset(record, 0, sizeof(RzRecord));
}

RZ_API const char* RzStatusMessage(RzStatus status) {
    switch (status) {
    case RZ_OK:
        return "Success";
    case RZ_ERR_INVALID_ARGUMENT:
        return "Invalid argument or null pointer";
    case RZ_ERR_BUFFER_TOO_SMALL:
        return "Output buffer capacity is insufficient";
    case RZ_ERR_MALFORMED_TEMPLATE:
        return "Template contains malformed placeholders or unescaped braces";
    case RZ_ERR_COUNT_MISMATCH:
        return "Number of provided values does not match template placeholders";
    case RZ_ERR_MESSAGE_TOO_LARGE:
        return "Message exceeds maximum allowed size";
    case RZ_ERR_OUT_OF_MEMORY:
        return "Out of memory";
    case RZ_ERR_INTERNAL:
        return "Internal engine error";
    default:
        return "Unknown error";
    }
}

RZ_API const char* RzVersion(void) {
    return LIBRAZOBILL_VERSION_STRING;
}

} // extern "C"
