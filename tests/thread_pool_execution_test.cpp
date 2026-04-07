#include "moo/MooScheduler.hpp"
#include "moo/TaskRegistry.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

using namespace std::chrono_literals;

class BlockingTask final : public moo::Task {
public:
    BlockingTask(std::string name,
                 std::atomic<int>& activeCount,
                 std::atomic<int>& maxObservedConcurrency,
                 std::mutex& startMutex,
                 std::vector<std::string>& startedTasks)
        : moo::Task(std::move(name),
                    10.0,
                    moo::TaskCosts{.cpu = 5.0, .memory = 5.0, .network = 0.0},
                    moo::ExecutionMode::OneShot,
                    250ms),
          activeCount_(activeCount),
          maxObservedConcurrency_(maxObservedConcurrency),
          startMutex_(startMutex),
          startedTasks_(startedTasks) {}

    void run() override {
        {
            std::scoped_lock lock{startMutex_};
            startedTasks_.push_back(name());
        }

        const int activeNow = activeCount_.fetch_add(1) + 1;
        int observed = maxObservedConcurrency_.load();
        while (activeNow > observed && !maxObservedConcurrency_.compare_exchange_weak(observed, activeNow)) {
        }

        std::this_thread::sleep_for(duration());
        activeCount_.fetch_sub(1);
    }

private:
    std::atomic<int>& activeCount_;
    std::atomic<int>& maxObservedConcurrency_;
    std::mutex& startMutex_;
    std::vector<std::string>& startedTasks_;
};

}  // namespace

int main() {
    try {
        moo::TaskRegistry registry;
        std::atomic<int> activeCount{0};
        std::atomic<int> maxObservedConcurrency{0};
        std::mutex startMutex;
        std::vector<std::string> startedTasks;

        for (int i = 0; i < 12; ++i) {
            registry.registerTask(std::make_shared<BlockingTask>(
                "blocking-" + std::to_string(i),
                activeCount,
                maxObservedConcurrency,
                startMutex,
                startedTasks));
        }

        moo::MooScheduler scheduler{registry};
        const auto plan = scheduler.buildSchedule(moo::OptimizationSettings{});

        scheduler.execute(plan,
                          moo::ExecutionSettings{
                              .runFor = 2s,
                              .maxCpu = 100.0,
                              .maxMemory = 100.0,
                              .maxThreads = 3,
                              .schedulerTick = 5ms,
                          });

        if (startedTasks.size() != 12) {
            throw std::runtime_error("Expected all 12 one-shot tasks to execute");
        }

        const int concurrency = maxObservedConcurrency.load();
        if (concurrency < 2) {
            throw std::runtime_error("Expected real concurrent execution, observed concurrency < 2");
        }
        if (concurrency > 3) {
            throw std::runtime_error("Thread pool limit violated; observed concurrency exceeded configured maxThreads");
        }

        std::cout << "observed max concurrency=" << concurrency << '\n';
        std::cout << "thread_pool_execution_test passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& ex) {
        std::cerr << "thread_pool_execution_test failed: " << ex.what() << '\n';
        return EXIT_FAILURE;
    }
}
