// ==============================================================================
// Razobill Universal C ABI Implementation
// ==============================================================================

#include "razobill.h"
#include "librazobill/Slots.hpp"

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

RAZOBILL_API razobill_status_t razobill_split(
    const char* message,
    size_t message_len,
    razobill_record_t* out_record
) {
    if (!out_record) return RAZOBILL_ERR_INVALID_ARGUMENT;
    if (!message && message_len > 0) return RAZOBILL_ERR_INVALID_ARGUMENT;

    std::memset(out_record, 0, sizeof(razobill_record_t));

    if (message_len == 0) {
        static const char emptyStr = '\0';
        out_record->template_str = &emptyStr;
        out_record->template_len = 0;
        return RAZOBILL_OK;
    }

    try {
        auto internal = std::make_unique<InternalRecord>();
        const std::string_view msgView(message, message_len);

        const razobill::Status status = razobill::core::parse(msgView, internal->slots, razobill::core::ScanMode::Semantic);
        if (status != razobill::Status::Ok) {
            if (status == razobill::Status::TooLarge) return RAZOBILL_ERR_MESSAGE_TOO_LARGE;
            if (status == razobill::Status::Malformed) return RAZOBILL_ERR_MALFORMED_TEMPLATE;
            return RAZOBILL_ERR_INVALID_ARGUMENT;
        }

        const auto& strs = internal->slots.strs;
        internal->strPointers.reserve(strs.size());
        internal->strLengths.reserve(strs.size());
        for (const auto& sv : strs) {
            internal->strPointers.push_back(sv.data());
            internal->strLengths.push_back(sv.size());
        }

        out_record->template_str = internal->slots.templateStr.data();
        out_record->template_len = internal->slots.templateStr.size();
        out_record->ints = internal->slots.ints.data();
        out_record->int_count = internal->slots.ints.size();
        out_record->strs = internal->strPointers.data();
        out_record->str_lens = internal->strLengths.data();
        out_record->str_count = internal->strPointers.size();
        out_record->internal_handle = internal.release();

        return RAZOBILL_OK;
    } catch (const std::bad_alloc&) {
        return RAZOBILL_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return RAZOBILL_ERR_INTERNAL;
    }
}

RAZOBILL_API razobill_status_t razobill_join(
    const razobill_record_t* record,
    char* out_buf,
    size_t buf_cap,
    size_t* out_len
) {
    if (!record) return RAZOBILL_ERR_INVALID_ARGUMENT;
    if (!record->template_str && record->template_len > 0) return RAZOBILL_ERR_INVALID_ARGUMENT;

    try {
        const std::string_view tmplView(record->template_str ? record->template_str : "", record->template_len);
        std::span<const int64_t> intSpan(record->ints ? record->ints : nullptr, record->int_count);

        std::vector<std::string_view> strViews;
        strViews.reserve(record->str_count);
        for (size_t i = 0; i < record->str_count; ++i) {
            const char* sPtr = record->strs ? record->strs[i] : nullptr;
            const size_t sLen = record->str_lens ? record->str_lens[i] : 0;
            if (!sPtr && sLen > 0) return RAZOBILL_ERR_INVALID_ARGUMENT;
            strViews.emplace_back(sPtr ? sPtr : "", sLen);
        }

        std::string result;
        const razobill::Status status = razobill::core::reconstruct(tmplView, intSpan, strViews, result);
        if (status != razobill::Status::Ok) {
            if (status == razobill::Status::SlotUnderflow) return RAZOBILL_ERR_COUNT_MISMATCH;
            if (status == razobill::Status::Malformed) return RAZOBILL_ERR_MALFORMED_TEMPLATE;
            if (status == razobill::Status::TooLarge) return RAZOBILL_ERR_MESSAGE_TOO_LARGE;
            return RAZOBILL_ERR_INVALID_ARGUMENT;
        }

        const size_t req_len = result.size();
        if (out_len) *out_len = req_len;

        if (!out_buf || buf_cap < req_len) {
            return RAZOBILL_ERR_BUFFER_TOO_SMALL;
        }

        if (req_len > 0) {
            std::memcpy(out_buf, result.data(), req_len);
        }
        return RAZOBILL_OK;
    } catch (const std::bad_alloc&) {
        return RAZOBILL_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return RAZOBILL_ERR_INTERNAL;
    }
}

RAZOBILL_API razobill_status_t razobill_template(
    const char* message,
    size_t message_len,
    char* out_buf,
    size_t buf_cap,
    size_t* out_len
) {
    if (!message && message_len > 0) return RAZOBILL_ERR_INVALID_ARGUMENT;

    try {
        if (message_len == 0) {
            if (out_len) *out_len = 0;
            return RAZOBILL_OK;
        }

        razobill::core::SlotRecord slots;
        const std::string_view msgView(message, message_len);
        const razobill::Status status = razobill::core::parse(msgView, slots, razobill::core::ScanMode::Semantic);
        if (status != razobill::Status::Ok) {
            if (status == razobill::Status::TooLarge) return RAZOBILL_ERR_MESSAGE_TOO_LARGE;
            if (status == razobill::Status::Malformed) return RAZOBILL_ERR_MALFORMED_TEMPLATE;
            return RAZOBILL_ERR_INVALID_ARGUMENT;
        }

        const size_t req_len = slots.templateStr.size();
        if (out_len) *out_len = req_len;

        if (!out_buf || buf_cap < req_len) {
            return RAZOBILL_ERR_BUFFER_TOO_SMALL;
        }

        if (req_len > 0) {
            std::memcpy(out_buf, slots.templateStr.data(), req_len);
        }
        return RAZOBILL_OK;
    } catch (const std::bad_alloc&) {
        return RAZOBILL_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return RAZOBILL_ERR_INTERNAL;
    }
}

RAZOBILL_API void razobill_free(razobill_record_t* record) {
    if (!record) return;
    if (record->internal_handle) {
        delete static_cast<InternalRecord*>(record->internal_handle);
        record->internal_handle = nullptr;
    }
    std::memset(record, 0, sizeof(razobill_record_t));
}

RAZOBILL_API const char* razobill_status_message(razobill_status_t status) {
    switch (status) {
    case RAZOBILL_OK:
        return "Success";
    case RAZOBILL_ERR_INVALID_ARGUMENT:
        return "Invalid argument or null pointer";
    case RAZOBILL_ERR_BUFFER_TOO_SMALL:
        return "Output buffer capacity is insufficient";
    case RAZOBILL_ERR_MALFORMED_TEMPLATE:
        return "Template contains malformed placeholders or unescaped braces";
    case RAZOBILL_ERR_COUNT_MISMATCH:
        return "Number of provided values does not match template placeholders";
    case RAZOBILL_ERR_MESSAGE_TOO_LARGE:
        return "Message exceeds maximum allowed size";
    case RAZOBILL_ERR_OUT_OF_MEMORY:
        return "Out of memory";
    case RAZOBILL_ERR_INTERNAL:
        return "Internal engine error";
    default:
        return "Unknown error";
    }
}

RAZOBILL_API const char* razobill_version(void) {
    return "2.0.0";
}

} // extern "C"
