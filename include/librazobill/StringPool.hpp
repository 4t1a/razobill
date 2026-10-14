// High-efficiency string pool and text dictionary with GC and compaction support.
// Data-Oriented Design (DOD): C++20 heterogeneous string_view hashing and reader-writer locking.
#pragma once

#include <cstdint>
#include <deque>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace razobill::core {

struct StringPoolStats {
    std::size_t totalBefore = 0;
    std::size_t activeAfter = 0;
    std::size_t purgedCount = 0;
};

namespace detail {
struct TransparentStringHash {
    using is_transparent = void;
    [[nodiscard]] std::size_t operator()(std::string_view sv) const noexcept {
        return std::hash<std::string_view>{}(sv);
    }
    [[nodiscard]] std::size_t operator()(const std::string& s) const noexcept {
        return std::hash<std::string_view>{}(s);
    }
};
struct TransparentStringEqual {
    using is_transparent = void;
    [[nodiscard]] bool operator()(std::string_view a, std::string_view b) const noexcept { return a == b; }
    [[nodiscard]] bool operator()(const std::string& a, const std::string& b) const noexcept { return a == b; }
    [[nodiscard]] bool operator()(const std::string& a, std::string_view b) const noexcept { return a == b; }
    [[nodiscard]] bool operator()(std::string_view a, const std::string& b) const noexcept { return a == b; }
};
} // namespace detail

class StringPool {
public:
    StringPool() = default;

    // Interns a string, returning a dense 32-bit integer ID [0..N-1].
    // Utilizes shared_mutex fast-path with zero heap allocation for existing strings.
    std::uint32_t intern(std::string_view s) {
        {
            std::shared_lock<std::shared_mutex> lock(mutex_);
            auto it = strToId_.find(s);
            if (it != strToId_.end()) {
                return it->second;
            }
        }

        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto it = strToId_.find(s);
        if (it != strToId_.end()) {
            return it->second;
        }

        std::uint32_t id = static_cast<std::uint32_t>(idToStr_.size());
        std::string strCopy(s);
        strToId_[strCopy] = id;
        idToStr_.push_back(std::move(strCopy));
        return id;
    }

    // Fast string_view accessor preserving source compatibility.
    // References remain stable across intern() operations because idToStr_ uses std::deque
    // (guaranteeing pointer stability on push_back). Invalidated only on gc() or clear().
    [[nodiscard]] std::string_view lookup(std::uint32_t id) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        if (id < idToStr_.size()) {
            return idToStr_[id];
        }
        return {};
    }

    // Thread-safe owning copy under read lock.
    [[nodiscard]] std::string lookupCopy(std::uint32_t id) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        if (id < idToStr_.size()) {
            return idToStr_[id];
        }
        return {};
    }

    // Thread-safe buffer assignment under read lock.
    bool lookup(std::uint32_t id, std::string& out) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        if (id < idToStr_.size()) {
            out = idToStr_[id];
            return true;
        }
        return false;
    }

    // Zero-allocation scoped inspection accessor: holds the read lock during callback execution.
    // CONTRACT: The callback `fn` MUST NOT invoke mutating methods (such as intern() or clear())
    // on the same StringPool instance, as std::shared_mutex is not recursive and attempting to
    // acquire an exclusive lock while holding a shared lock will result in deadlock.
    template <typename F> bool withString(std::uint32_t id, F&& fn) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        if (id < idToStr_.size()) {
            fn(std::string_view(idToStr_[id]));
            return true;
        }
        return false;
    }

    [[nodiscard]] std::size_t size() const noexcept {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return idToStr_.size();
    }

    void clear() noexcept {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        strToId_.clear();
        idToStr_.clear();
    }

    // Garbage collection: purges strings not present in activeIds set, reindexing compactly.
    template <typename SetType> StringPoolStats gc(const SetType& activeIds) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        StringPoolStats stats;
        stats.totalBefore = idToStr_.size();

        std::deque<std::string> newIdToStr;
        std::unordered_map<std::string, std::uint32_t, detail::TransparentStringHash, detail::TransparentStringEqual>
            newStrToId;

        for (std::uint32_t id = 0; id < static_cast<std::uint32_t>(idToStr_.size()); ++id) {
            if (activeIds.find(id) != activeIds.end()) {
                std::uint32_t newId = static_cast<std::uint32_t>(newIdToStr.size());
                newStrToId[idToStr_[id]] = newId;
                newIdToStr.push_back(std::move(idToStr_[id]));
            }
        }

        stats.activeAfter = newIdToStr.size();
        stats.purgedCount = stats.totalBefore - stats.activeAfter;

        idToStr_ = std::move(newIdToStr);
        strToId_ = std::move(newStrToId);
        return stats;
    }

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, std::uint32_t, detail::TransparentStringHash, detail::TransparentStringEqual>
        strToId_;
    std::deque<std::string> idToStr_;
};

} // namespace razobill::core
