#include "moo/MooScheduler.hpp"

#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

using namespace moo;
using namespace std::chrono_literals;

class LoggingTask : public Task {
public:
    LoggingTask()
        : Task("Logging",
               6.0,
               TaskCosts{.cpu = 18.0, .memory = 15.0, .network = 0.1},
               ExecutionMode::Daemon,
               600ms,
               1500ms) {}

    void run() override {
        std::cout << name() << " running for " << duration().count() << " ms" << std::endl;
        std::this_thread::sleep_for(duration());
    }
};

class ReconTask : public Task {
public:
    ReconTask()
        : Task("Recon",
               10.0,
               TaskCosts{.cpu = 30.0, .memory = 25.0, .network = 2.0},
               ExecutionMode::OneShot,
               900ms) {}

    void run() override {
        std::cout << name() << " running for " << duration().count() << " ms" << std::endl;
        std::this_thread::sleep_for(duration());
    }
};

class CompressionTask : public Task {
public:
    CompressionTask()
        : Task("Compression",
               4.0,
               TaskCosts{.cpu = 24.0, .memory = 28.0, .network = 0.3},
               ExecutionMode::OneShot,
               700ms) {}

    void run() override {
        std::cout << name() << " running for " << duration().count() << " ms" << std::endl;
        std::this_thread::sleep_for(duration());
    }
};

int main() {
    TaskRegistry registry;
    registry.registerTask(std::make_shared<LoggingTask>());
    registry.registerTask(std::make_shared<ReconTask>());
    registry.registerTask(std::make_shared<CompressionTask>());

    MooScheduler scheduler{registry};
    OptimizationSettings settings{
        .populationSize = 32,
        .generations = 40,
        .crossoverProbability = 0.9,
        .mutationProbability = 0.1,
    };

    auto plan = scheduler.buildSchedule(settings);

    std::cout << "Selected " << plan.oneShotTasks.size() << " one-shot task(s) and "
              << plan.daemonTasks.size() << " daemon(s)" << std::endl;
    std::cout << "Benefit=" << plan.summary.missionBenefit
              << " CPU=" << plan.summary.cpuCost
              << " MEM=" << plan.summary.memoryCost
              << " NET=" << plan.summary.networkCost << std::endl;

    scheduler.execute(plan, ExecutionSettings{.runFor = 10s, .maxCpu = 100.0, .maxMemory = 100.0, .schedulerTick = 25ms});
    return 0;
}
