#include "moo/MooScheduler.hpp"

#include <iostream>
#include <memory>

using namespace moo;

class LoggingTask : public Task {
public:
    LoggingTask()
        : Task("Logging", 6.0, TaskCosts{.cpu = 1.0, .memory = 0.5, .network = 0.1}, ExecutionMode::Daemon) {}

    void run() override {
        std::cout << "LoggingTask::run()" << std::endl;
    }
};

class ReconTask : public Task {
public:
    ReconTask()
        : Task("Recon", 10.0, TaskCosts{.cpu = 3.0, .memory = 1.5, .network = 2.0}, ExecutionMode::OneShot) {}

    void run() override {
        std::cout << "ReconTask::run()" << std::endl;
    }
};

class CompressionTask : public Task {
public:
    CompressionTask()
        : Task("Compression", 4.0, TaskCosts{.cpu = 2.0, .memory = 2.5, .network = 0.3}, ExecutionMode::OneShot) {}

    void run() override {
        std::cout << "CompressionTask::run()" << std::endl;
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

    scheduler.execute(plan);
    return 0;
}
