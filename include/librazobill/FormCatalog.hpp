// FormCatalog: Thread-safe cache and registry for mined form templates.
// Implements full-key canonical string comparison with 64-bit xxHash64 indexing.
// Guarantees zero data loss and collision disambiguation under the Birthday Bound.
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
    std::uint32_t id = 0; // Dense unique sequential identifier (1-based)
    std::uint64_t hash = 0;
    std::string key;
    std::string templateStr;
    std::uint32_t intSlots = 0;
    std::uint32_t strSlots = 0;
};

namespace detail {
struct FormKeyHash {
    using is_transparent = void;
    [[nodiscard]] std::size_t operator()(std::string_view sv) const noexcept {
        return std::hash<std::string_view>{}(sv);
    }
    [[nodiscard]] std::size_t operator()(const std::string& s) const noexcept {
        return std::hash<std::string_view>{}(s);
    }
};

struct FormKeyEqual {
    using is_transparent = void;
    [[nodiscard]] bool operator()(std::string_view a, std::string_view b) const noexcept { return a == b; }
    [[nodiscard]] bool operator()(const std::string& a, const std::string& b) const noexcept { return a == b; }
    [[nodiscard]] bool operator()(const std::string& a, std::string_view b) const noexcept { return a == b; }
    [[nodiscard]] bool operator()(std::string_view a, const std::string& b) const noexcept { return a == b; }
};
} // namespace detail

class FormCatalog {
public:
    FormCatalog() = default;

    // Registers a form key and its template.
    // Computes xxHash64(key) as secondary fingerprint while using the full canonical key
    // for exact dictionary uniqueness.
    // CONTRACT: The returned 64-bit integer is the secondary fingerprint (xxHash64) of the form key.
    // Under the Birthday Bound, 64-bit hashes are susceptible to collision across large template sets.
    // This return value is NOT guaranteed to be globally unique. Callers requiring unambiguous dense handles
    // should use registerFormGetId() or lookup by dense ID via findFormById().
    std::uint64_t registerForm(std::string_view key, std::string_view tmpl, std::uint32_t numInts,
                               std::uint32_t numStrs) {
        return registerFormWithHash(key, tmpl, numInts, numStrs, xxh64(key));
    }

    // Direct registration with explicit hash parameter (supports collision simulation & testing).
    // Returns explicitHash. Call registerFormWithHashGetId() to obtain the unique dense ID.
    std::uint64_t registerFormWithHash(std::string_view key, std::string_view tmpl, std::uint32_t numInts,
                                       std::uint32_t numStrs, std::uint64_t explicitHash) {
        (void)registerFormWithHashGetId(key, tmpl, numInts, numStrs, explicitHash);
        return explicitHash;
    }

    // Registers a form key, returning the dense unique sequential identifier (1-based id [1..N]).
    // The returned ID is guaranteed to be globally unique within this catalog instance, even under hash collisions.
    std::uint32_t registerFormGetId(std::string_view key, std::string_view tmpl, std::uint32_t numInts,
                                    std::uint32_t numStrs) {
        return registerFormWithHashGetId(key, tmpl, numInts, numStrs, xxh64(key));
    }

    // Direct registration returning dense unique ID with explicit hash parameter.
    std::uint32_t registerFormWithHashGetId(std::string_view key, std::string_view tmpl, std::uint32_t numInts,
                                            std::uint32_t numStrs, std::uint64_t explicitHash) {
        // Shared lock read fast-path with zero dynamic allocation
        {
            std::shared_lock<std::shared_mutex> lock(mutex_);
            auto it = formsByKey_.find(key);
            if (it != formsByKey_.end()) {
                return it->second.id;
            }
        }

        // Exclusive lock write path
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto it = formsByKey_.find(key);
        if (it != formsByKey_.end()) {
            return it->second.id;
        }

        auto hashIt = hashToKeys_.find(explicitHash);
        if (hashIt != hashToKeys_.end()) {
            // Hash collision: an existing entry has the same 64-bit hash but a distinct key!
            collisionCount_++;
        }

        FormEntry entry;
        entry.id = static_cast<std::uint32_t>(formsByKey_.size() + 1);
        entry.hash = explicitHash;
        entry.key = std::string(key);
        entry.templateStr = std::string(tmpl);
        entry.intSlots = numInts;
        entry.strSlots = numStrs;

        hashToKeys_[explicitHash].push_back(entry.key);
        auto [insertedIt, _] = formsByKey_.emplace(entry.key, std::move(entry));
        formsById_.push_back(&insertedIt->second);
        return insertedIt->second.id;
    }

    [[nodiscard]] bool contains(std::string_view key) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return formsByKey_.find(key) != formsByKey_.end();
    }

    [[nodiscard]] bool contains(std::uint64_t hash) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return hashToKeys_.find(hash) != hashToKeys_.end();
    }

    [[nodiscard]] bool findForm(std::string_view key, FormEntry& out) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = formsByKey_.find(key);
        if (it != formsByKey_.end()) {
            out = it->second;
            return true;
        }
        return false;
    }

    [[nodiscard]] bool findFormById(std::uint32_t id, FormEntry& out) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        if (id > 0 && id <= formsById_.size()) {
            out = *formsById_[id - 1];
            return true;
        }
        return false;
    }

    [[nodiscard]] bool findKeysByHash(std::uint64_t hash, std::vector<std::string>& outKeys) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = hashToKeys_.find(hash);
        if (it != hashToKeys_.end()) {
            outKeys = it->second;
            return true;
        }
        return false;
    }

    [[nodiscard]] bool isCollisionHash(std::uint64_t hash) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = hashToKeys_.find(hash);
        return it != hashToKeys_.end() && it->second.size() > 1;
    }

    [[nodiscard]] std::size_t size() const noexcept {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return formsByKey_.size();
    }

    [[nodiscard]] std::size_t collisionCount() const noexcept {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return collisionCount_;
    }

    void clear() noexcept {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        formsByKey_.clear();
        hashToKeys_.clear();
        formsById_.clear();
        collisionCount_ = 0;
    }

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, FormEntry, detail::FormKeyHash, detail::FormKeyEqual> formsByKey_;
    std::unordered_map<std::uint64_t, std::vector<std::string>> hashToKeys_;
    std::vector<const FormEntry*> formsById_;
    std::size_t collisionCount_ = 0;
};

} // namespace razobill::core
