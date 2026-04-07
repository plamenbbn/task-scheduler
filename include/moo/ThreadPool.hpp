#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace moo {

class ThreadPool {
public:
    explicit ThreadPool(std::size_t threadCount);
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    template <typename Fn>
    auto submit(Fn&& fn) -> std::future<std::invoke_result_t<Fn>> {
        using Result = std::invoke_result_t<Fn>;

        auto task = std::make_shared<std::packaged_task<Result()>>(std::forward<Fn>(fn));
        auto future = task->get_future();

        {
            std::scoped_lock lock{mutex_};
            if (stopping_) {
                throw std::runtime_error{"Cannot submit work to a stopped ThreadPool"};
            }
            tasks_.push([task]() { (*task)(); });
        }

        condition_.notify_one();
        return future;
    }

    [[nodiscard]] std::size_t size() const noexcept { return workers_.size(); }

private:
    void workerLoop();

    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::queue<std::function<void()>> tasks_;
    std::vector<std::thread> workers_;
    bool stopping_ = false;
};

}  // namespace moo
