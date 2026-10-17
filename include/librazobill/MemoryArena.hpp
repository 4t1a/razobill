// MemoryArena: Monotonic bump-pointer memory allocator with 64-byte cache-line alignment.
// Provides O(1) allocation, bulk reset with 100% block reuse (zero re-allocation churn),
// and strict memory safety. Grounded in Data-Oriented Design (DOD).
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <memory>
#include <new>
#include <utility>
#include <vector>

namespace razobill::core {

class MemoryArena {
public:
    struct Block {
        void* ptr = nullptr;
        std::size_t capacity = 0;
    };

    explicit MemoryArena(std::size_t defaultBlockSize = 256 * 1024) : defaultBlockSize_(defaultBlockSize) {}

    ~MemoryArena() { clear(); }

    MemoryArena(const MemoryArena&) = delete;
    MemoryArena& operator=(const MemoryArena&) = delete;

    MemoryArena(MemoryArena&& other) noexcept
        : blocks_(std::move(other.blocks_)), currentBlockIdx_(other.currentBlockIdx_), offset_(other.offset_),
          defaultBlockSize_(other.defaultBlockSize_) {
        other.currentBlockIdx_ = 0;
        other.offset_ = 0;
        other.defaultBlockSize_ = 0;
    }

    MemoryArena& operator=(MemoryArena&& other) noexcept {
        if (this != &other) {
            clear();
            blocks_ = std::move(other.blocks_);
            currentBlockIdx_ = other.currentBlockIdx_;
            offset_ = other.offset_;
            defaultBlockSize_ = other.defaultBlockSize_;
            other.currentBlockIdx_ = 0;
            other.offset_ = 0;
            other.defaultBlockSize_ = 0;
        }
        return *this;
    }

    // Fast monotonic bump allocation with alignment guarantees (default 8 bytes, or 64 for SIMD vectors).
    [[nodiscard]] void* allocate(std::size_t bytes, std::size_t alignment = 8) {
        if (bytes == 0) [[unlikely]]
            return nullptr;

        // Fast path: fits into active block
        if (currentBlockIdx_ < blocks_.size()) [[likely]] {
            const auto& cur = blocks_[currentBlockIdx_];
            const std::size_t currentAddr = reinterpret_cast<std::size_t>(cur.ptr) + offset_;
            const std::size_t alignedAddr = (currentAddr + alignment - 1) & ~(alignment - 1);
            const std::size_t padding = alignedAddr - currentAddr;

            if (bytes <= cur.capacity && offset_ + padding <= cur.capacity - bytes) [[likely]] {
                offset_ += padding + bytes;
                return reinterpret_cast<void*>(alignedAddr);
            }
        }

        // Slow path: scan existing subsequent blocks or allocate new block
        while (++currentBlockIdx_ < blocks_.size()) {
            offset_ = 0;
            const auto& cur = blocks_[currentBlockIdx_];
            const std::size_t currentAddr = reinterpret_cast<std::size_t>(cur.ptr);
            const std::size_t alignedAddr = (currentAddr + alignment - 1) & ~(alignment - 1);
            const std::size_t padding = alignedAddr - currentAddr;

            if (padding <= cur.capacity && bytes <= cur.capacity - padding) {
                offset_ = padding + bytes;
                return reinterpret_cast<void*>(alignedAddr);
            }
        }

        // Prevent integer overflow when computing block capacity
        if (bytes > std::numeric_limits<std::size_t>::max() - alignment - 64) [[unlikely]] {
            throw std::bad_alloc();
        }

        // Allocate a new OS block with 64-byte cache line alignment
        const std::size_t required = (bytes + alignment > defaultBlockSize_) ? (bytes + alignment) : defaultBlockSize_;
        void* newBlock = nullptr;
#if defined(_MSC_VER) || defined(__MINGW32__)
        newBlock = _aligned_malloc(required, 64);
#else
        if (posix_memalign(&newBlock, 64, required) != 0) newBlock = nullptr;
#endif
        if (!newBlock) [[unlikely]]
            throw std::bad_alloc();

        struct BlockGuard {
            void* ptr;
            ~BlockGuard() {
                if (ptr) {
#if defined(_MSC_VER) || defined(__MINGW32__)
                    _aligned_free(ptr);
#else
                    free(ptr);
#endif
                }
            }
        } guard{newBlock};

        blocks_.push_back({newBlock, required});
        guard.ptr = nullptr; // Disarm on successful vector push_back
        currentBlockIdx_ = blocks_.size() - 1;

        const std::size_t baseAddr = reinterpret_cast<std::size_t>(newBlock);
        const std::size_t alignedBase = (baseAddr + alignment - 1) & ~(alignment - 1);
        offset_ = (alignedBase - baseAddr) + bytes;
        return reinterpret_cast<void*>(alignedBase);
    }

    template <typename T, typename... Args> [[nodiscard]] T* create(Args&&... args) {
        static_assert(std::is_trivially_destructible_v<T>,
                      "MemoryArena::create only supports trivially destructible types (destructors are not called on reset)");
        void* mem = allocate(sizeof(T), alignof(T));
        return ::new (mem) T(std::forward<Args>(args)...);
    }

    // O(1) bulk reset: rewinds bump pointer to zero across all blocks without deallocating pages.
    // Preserves warm L1/L2 caches and eliminates kernel page faults.
    void reset() noexcept {
        currentBlockIdx_ = 0;
        offset_ = 0;
    }

    // Frees all backing memory blocks back to the OS.
    void clear() noexcept {
        for (const auto& block : blocks_) {
            if (block.ptr) {
#if defined(_MSC_VER) || defined(__MINGW32__)
                _aligned_free(block.ptr);
#else
                std::free(block.ptr);
#endif
            }
        }
        blocks_.clear();
        currentBlockIdx_ = 0;
        offset_ = 0;
    }

    [[nodiscard]] std::size_t totalAllocatedBlocks() const noexcept { return blocks_.size(); }

    [[nodiscard]] std::size_t totalAllocatedCapacity() const noexcept {
        std::size_t sum = 0;
        for (const auto& b : blocks_)
            sum += b.capacity;
        return sum;
    }

    [[nodiscard]] std::size_t capacity() const noexcept { return totalAllocatedCapacity(); }

    [[nodiscard]] std::size_t activeBlockIndex() const noexcept { return currentBlockIdx_; }

    [[nodiscard]] std::size_t currentOffset() const noexcept { return offset_; }

    [[nodiscard]] std::size_t allocatedBytes() const noexcept {
        if (blocks_.empty()) return 0;
        std::size_t sum = 0;
        for (std::size_t i = 0; i < currentBlockIdx_; ++i) {
            sum += blocks_[i].capacity;
        }
        sum += offset_;
        return sum;
    }

private:
    std::vector<Block> blocks_;
    std::size_t currentBlockIdx_ = 0;
    std::size_t offset_ = 0;
    std::size_t defaultBlockSize_;
};

} // namespace razobill::core
