// FormCatalog: Thread-safe cache and registry for mined form templates.
// Uses 64-bit xxHash64 fingerprinting and shared_mutex read fast-path.
#pragma once

#include <cstdint>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "librazobill/Hash.hpp"

namespace razobill::core {

struct FormEntry {
    std::uint64_t hash = 0;
    std::string key;
    std::string templateStr;
    std::uint32_t intSlots = 0;
    std::uint32_t strSlots = 0;
};

class FormCatalog {
public:
    FormCatalog() = default;

    std::uint64_t registerForm(std::string_view key, std::string_view tmpl, std::uint32_t numInts,
                               std::uint32_t numStrs) {
        const std::uint64_t h = xxh64(key);
        {
            std::shared_lock<std::shared_mutex> lock(mutex_);
            if (forms_.find(h) != forms_.end()) {
                return h;
            }
        }

        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto it = forms_.find(h);
        if (it == forms_.end()) {
            FormEntry entry;
            entry.hash = h;
            entry.key = std::string(key);
            entry.templateStr = std::string(tmpl);
            entry.intSlots = numInts;
            entry.strSlots = numStrs;
            forms_[h] = std::move(entry);
        }
        return h;
    }

    [[nodiscard]] bool contains(std::uint64_t hash) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return forms_.find(hash) != forms_.end();
    }

    [[nodiscard]] std::size_t size() const noexcept {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return forms_.size();
    }

    void clear() noexcept {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        forms_.clear();
    }

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::uint64_t, FormEntry> forms_;
};

} // namespace razobill::core
