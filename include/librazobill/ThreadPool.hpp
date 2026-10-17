// ThreadPool: Cache-aware parallel worker pool for multi-core chunked processing.
//
// CONCURRENCY & MECHANICAL SYMPATHY:
// When parsing 70+ million messages across 12 cores, how you schedule work determines whether
// you get 2 million msgs/sec or 69 million msgs/sec.
//
// THE ROOKIE MISTAKE:
// Dispatching 1 task per log line into a thread pool. A mutex lock takes ~15-25 nanoseconds.
// If your parsing routine takes 100 nanoseconds, workers spend 20% of their time fighting
// over the queue mutex!
//
// THE HARDENED APPROACH:
// We use `parallelFor` with coarse chunks (e.g. 5,000 to 10,000 items per chunk).
// A worker locks the queue ONCE, grabs a 10,000-line span, and crunches through it
// at full SIMD L1 cache bandwidth with ZERO cross-thread synchronization.
//
// JOKE:
// Never acquire a mutex inside a per-row loop unless you want your hardware threads
// to spend their working hours queuing up at the DMV.
#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace razobill::core {

class ThreadPool {
public:
    // Auto-detects available hardware threads (logical cores).
    // If the OS returns 0 (can happen in weird containerized cgroups), safely fall back to 1.
    explicit ThreadPool(std::size_t threadCount = std::thread::hardware_concurrency()) : stopping_(false) {
        if (threadCount == 0) threadCount = 1;
        try {
            workers_.reserve(threadCount);
            for (std::size_t i = 0; i < threadCount; ++i) {
                workers_.emplace_back([this]() {
                    while (true) {
                        std::function<void()> task;
                        {
                            std::unique_lock<std::mutex> lock(mutex_);
                            cv_.wait(lock, [this]() { return stopping_ || !tasks_.empty(); });
                            if (stopping_ && tasks_.empty()) return;
                            task = std::move(tasks_.front());
                            tasks_.pop();
                        }
                        task();
                    }
                });
            }
        } catch (...) {
            {
                std::unique_lock<std::mutex> lock(mutex_);
                stopping_ = true;
            }
            cv_.notify_all();
            for (auto& worker : workers_) {
                if (worker.joinable()) worker.join();
            }
            throw;
        }
    }

    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            stopping_ = true;
        }
        cv_.notify_all();
        for (auto& worker : workers_) {
            if (worker.joinable()) worker.join();
        }
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    template <typename F> auto submit(F&& f) -> std::future<typename std::invoke_result<F>::type> {
        using ReturnType = typename std::invoke_result<F>::type;
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(std::forward<F>(f));
        std::future<ReturnType> res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(mutex_);
            if (stopping_) throw std::runtime_error("ThreadPool is stopping");
            tasks_.emplace([task]() { (*task)(); });
        }
        cv_.notify_one();
        return res;
    }

    template <typename F, typename... Args>
        requires(sizeof...(Args) > 0)
    auto submit(F&& f, Args&&... args)
        -> std::future<typename std::invoke_result<std::decay_t<F>, std::decay_t<Args>&...>::type> {
        using ReturnType = typename std::invoke_result<std::decay_t<F>, std::decay_t<Args>&...>::type;
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        std::future<ReturnType> res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(mutex_);
            if (stopping_) throw std::runtime_error("ThreadPool is stopping");
            tasks_.emplace([task]() { (*task)(); });
        }
        cv_.notify_one();
        return res;
    }

    template <typename F> void parallelFor(std::size_t total, std::size_t chunkSize, F&& fn) {
        if (total == 0) return;
        if (chunkSize == 0) {
            const std::size_t count = workers_.empty() ? 1 : workers_.size();
            chunkSize = std::max<std::size_t>(1, (total + count - 1) / count);
        }
        if (workers_.empty() || total <= chunkSize) {
            fn(0, total);
            return;
        }

        std::size_t numChunks = (total + chunkSize - 1) / chunkSize;
        std::vector<std::future<void>> futures;
        futures.reserve(numChunks);

        std::exception_ptr firstException = nullptr;
        try {
            for (std::size_t chunkIdx = 0; chunkIdx < numChunks; ++chunkIdx) {
                std::size_t start = chunkIdx * chunkSize;
                std::size_t end = std::min(start + chunkSize, total);
                futures.push_back(submit([&fn, start, end]() { fn(start, end); }));
            }
        } catch (...) {
            firstException = std::current_exception();
        }

        for (auto& fut : futures) {
            try {
                fut.get();
            } catch (...) {
                if (!firstException) {
                    firstException = std::current_exception();
                }
            }
        }

        if (firstException) {
            std::rethrow_exception(firstException);
        }
    }

    [[nodiscard]] std::size_t workerCount() const noexcept { return workers_.size(); }

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stopping_;
};

} // namespace razobill::core
