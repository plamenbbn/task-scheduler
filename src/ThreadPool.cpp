#include "moo/ThreadPool.hpp"

#include <stdexcept>

namespace moo {

ThreadPool::ThreadPool(std::size_t threadCount) {
    if (threadCount == 0) {
        throw std::invalid_argument{"ThreadPool requires at least one worker thread"};
    }

    workers_.reserve(threadCount);
    for (std::size_t i = 0; i < threadCount; ++i) {
        workers_.emplace_back([this]() {
            workerLoop();
        });
    }
}

ThreadPool::~ThreadPool() {
    {
        std::scoped_lock lock{mutex_};
        stopping_ = true;
    }

    condition_.notify_all();

    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void ThreadPool::workerLoop() {
    while (true) {
        std::function<void()> task;

        {
            std::unique_lock lock{mutex_};
            condition_.wait(lock, [this]() {
                return stopping_ || !tasks_.empty();
            });

            if (stopping_ && tasks_.empty()) {
                return;
            }

            task = std::move(tasks_.front());
            tasks_.pop();
        }

        task();
    }
}

}  // namespace moo
