#include "moo/MooScheduler.hpp"
#include "moo/TaskRegistry.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

namespace {

using namespace std::chrono_literals;

struct ExecutionTrace {
    std::mutex mutex;
    std::vector<std::string> entries;

    void log(const std::string& entry) {
        std::scoped_lock lock{mutex};
        entries.push_back(entry);
    }
};

class TestTask final : public moo::Task {
public:
    TestTask(std::string name,
             double missionBenefit,
             moo::TaskCosts costs,
             moo::ExecutionMode mode,
             std::chrono::milliseconds duration,
             ExecutionTrace& trace,
             std::chrono::milliseconds daemonFrequency = 0ms) noexcept
        : moo::Task(std::move(name), missionBenefit, costs, mode, duration, daemonFrequency),
          trace_(trace) {}

    void run() override {
        std::cout << name() << " running for " << duration().count() << " ms" << std::endl;
        trace_.log(name() + ":" + std::to_string(duration().count()));
        std::this_thread::sleep_for(duration());
    }

private:
    ExecutionTrace& trace_;
};

std::shared_ptr<TestTask> makeTask(std::size_t index,
                                   double missionBenefit,
                                   moo::TaskCosts costs,
                                   moo::ExecutionMode mode,
                                   std::chrono::milliseconds duration,
                                   ExecutionTrace& trace,
                                   std::chrono::milliseconds daemonFrequency = 0ms) {
    return std::make_shared<TestTask>(
        "task-" + std::to_string(index),
        missionBenefit,
        costs,
        mode,
        duration,
        trace,
        daemonFrequency);
}

void registerInitialTasks(moo::TaskRegistry& registry, ExecutionTrace& trace) {
    registry.registerTask(makeTask(0, 62.0, {.cpu = 30.0, .memory = 24.0, .network = 8.0}, moo::ExecutionMode::OneShot, 850ms, trace));
    registry.registerTask(makeTask(1, 58.0, {.cpu = 28.0, .memory = 26.0, .network = 6.0}, moo::ExecutionMode::OneShot, 780ms, trace));
    registry.registerTask(makeTask(2, 90.0, {.cpu = 20.0, .memory = 18.0, .network = 4.0}, moo::ExecutionMode::Daemon, 700ms, trace, 1200ms));
    registry.registerTask(makeTask(3, 54.0, {.cpu = 26.0, .memory = 32.0, .network = 7.0}, moo::ExecutionMode::OneShot, 960ms, trace));
    registry.registerTask(makeTask(4, 88.0, {.cpu = 22.0, .memory = 20.0, .network = 5.0}, moo::ExecutionMode::Daemon, 640ms, trace, 1600ms));
    registry.registerTask(makeTask(5, 47.0, {.cpu = 24.0, .memory = 29.0, .network = 9.0}, moo::ExecutionMode::OneShot, 620ms, trace));
    registry.registerTask(makeTask(6, 51.0, {.cpu = 34.0, .memory = 28.0, .network = 6.0}, moo::ExecutionMode::OneShot, 1020ms, trace));
    registry.registerTask(makeTask(7, 76.0, {.cpu = 18.0, .memory = 16.0, .network = 3.0}, moo::ExecutionMode::Daemon, 520ms, trace, 900ms));
    registry.registerTask(makeTask(8, 45.0, {.cpu = 20.0, .memory = 21.0, .network = 5.0}, moo::ExecutionMode::OneShot, 540ms, trace));
    registry.registerTask(makeTask(9, 69.0, {.cpu = 25.0, .memory = 24.0, .network = 5.0}, moo::ExecutionMode::Daemon, 830ms, trace, 1800ms));
}

void registerAdditionalTasks(moo::TaskRegistry& registry, ExecutionTrace& trace) {
    registry.registerTask(makeTask(10, 96.0, {.cpu = 16.0, .memory = 14.0, .network = 3.0}, moo::ExecutionMode::Daemon, 480ms, trace, 800ms));
    registry.registerTask(makeTask(11, 74.0, {.cpu = 19.0, .memory = 17.0, .network = 4.0}, moo::ExecutionMode::OneShot, 510ms, trace));
    registry.registerTask(makeTask(12, 91.0, {.cpu = 21.0, .memory = 20.0, .network = 3.0}, moo::ExecutionMode::Daemon, 600ms, trace, 1000ms));
    registry.registerTask(makeTask(13, 83.0, {.cpu = 17.0, .memory = 16.0, .network = 2.0}, moo::ExecutionMode::OneShot, 430ms, trace));
    registry.registerTask(makeTask(14, 78.0, {.cpu = 15.0, .memory = 18.0, .network = 2.0}, moo::ExecutionMode::Daemon, 560ms, trace, 950ms));
    registry.registerTask(makeTask(15, 67.0, {.cpu = 18.0, .memory = 19.0, .network = 4.0}, moo::ExecutionMode::OneShot, 650ms, trace));
    registry.registerTask(makeTask(16, 72.0, {.cpu = 14.0, .memory = 13.0, .network = 2.0}, moo::ExecutionMode::Daemon, 500ms, trace, 1100ms));
    registry.registerTask(makeTask(17, 64.0, {.cpu = 20.0, .memory = 21.0, .network = 5.0}, moo::ExecutionMode::OneShot, 740ms, trace));
    registry.registerTask(makeTask(18, 86.0, {.cpu = 18.0, .memory = 17.0, .network = 3.0}, moo::ExecutionMode::Daemon, 620ms, trace, 1050ms));
    registry.registerTask(makeTask(19, 70.0, {.cpu = 16.0, .memory = 18.0, .network = 4.0}, moo::ExecutionMode::OneShot, 580ms, trace));
}

double priorityScore(const moo::Task& task) {
    const double totalCost = task.cpuCost() + task.memoryCost() + task.networkCost() + 1e-9;
    return task.missionBenefit() / totalCost;
}

std::vector<std::shared_ptr<moo::Task>> collectPlannedTasks(const moo::SchedulePlan& plan) {
    std::vector<std::shared_ptr<moo::Task>> tasks;
    tasks.reserve(plan.oneShotTasks.size() + plan.daemonTasks.size());
    tasks.insert(tasks.end(), plan.oneShotTasks.begin(), plan.oneShotTasks.end());
    tasks.insert(tasks.end(), plan.daemonTasks.begin(), plan.daemonTasks.end());
    std::sort(tasks.begin(), tasks.end(), [](const auto& lhs, const auto& rhs) {
        const double lhsPriority = priorityScore(*lhs);
        const double rhsPriority = priorityScore(*rhs);
        if (lhsPriority == rhsPriority) {
            return lhs->name() < rhs->name();
        }
        return lhsPriority > rhsPriority;
    });
    return tasks;
}

void printPlan(const std::string& label, const moo::SchedulePlan& plan) {
    const auto plannedTasks = collectPlannedTasks(plan);

    std::cout << label << "\n";
    std::cout << "summary: benefit=" << std::fixed << std::setprecision(4) << plan.summary.missionBenefit
              << ", cpu=" << plan.summary.cpuCost
              << ", memory=" << plan.summary.memoryCost
              << ", network=" << plan.summary.networkCost << '\n';

    for (std::size_t index = 0; index < plannedTasks.size(); ++index) {
        const auto& task = plannedTasks[index];
        std::cout << (index + 1)
                  << ". name=" << task->name()
                  << ", priority=" << priorityScore(*task)
                  << ", benefit=" << task->missionBenefit()
                  << ", cpu=" << task->cpuCost()
                  << ", memory=" << task->memoryCost()
                  << ", network=" << task->networkCost()
                  << ", duration_ms=" << task->duration().count()
                  << ", mode=" << (task->mode() == moo::ExecutionMode::Daemon ? "daemon" : "one-shot");
        if (task->mode() == moo::ExecutionMode::Daemon) {
            std::cout << ", frequency_ms=" << task->daemonFrequency().count();
        }
        std::cout << '\n';
    }

    std::cout << '\n';
}

void validatePlanAgainstRegistry(const moo::TaskRegistry& registry,
                                 const moo::SchedulePlan& plan,
                                 std::size_t expectedRegistrySize) {
    const auto& registeredTasks = registry.tasks();
    if (registeredTasks.size() != expectedRegistrySize) {
        throw std::runtime_error("Registry size mismatch");
    }

    const auto plannedTasks = collectPlannedTasks(plan);
    if (plannedTasks.empty()) {
        throw std::runtime_error("Expected optimizer to schedule at least one task");
    }

    std::unordered_set<const moo::Task*> seen;
    double benefit = 0.0;
    double cpu = 0.0;
    double memory = 0.0;
    double network = 0.0;

    for (const auto& task : plannedTasks) {
        const moo::Task* raw = task.get();
        if (!seen.insert(raw).second) {
            throw std::runtime_error("Duplicate task found in schedule");
        }

        const bool existsInRegistry = std::any_of(registeredTasks.begin(), registeredTasks.end(),
                                                  [raw](const auto& registered) {
                                                      return registered.get() == raw;
                                                  });
        if (!existsInRegistry) {
            throw std::runtime_error("Scheduled task was not found in registry");
        }

        benefit += task->missionBenefit();
        cpu += task->cpuCost();
        memory += task->memoryCost();
        network += task->networkCost();
    }

    const auto approxEqual = [](double lhs, double rhs) {
        constexpr double tolerance = 1e-9;
        return std::abs(lhs - rhs) <= tolerance;
    };

    if (!approxEqual(plan.summary.missionBenefit, benefit) ||
        !approxEqual(plan.summary.cpuCost, cpu) ||
        !approxEqual(plan.summary.memoryCost, memory) ||
        !approxEqual(plan.summary.networkCost, network)) {
        throw std::runtime_error("Plan summary does not match scheduled tasks");
    }
}

}  // namespace

int main() {
    try {
        ExecutionTrace trace;
        moo::TaskRegistry registry;

        registerInitialTasks(registry, trace);

        moo::MooScheduler scheduler(registry);
        const moo::OptimizationSettings settings{
            .populationSize = 96,
            .generations = 120,
            .crossoverProbability = 0.9,
            .mutationProbability = 0.12,
        };

        const auto firstPlan = scheduler.buildSchedule(settings);
        validatePlanAgainstRegistry(registry, firstPlan, 10);
        printPlan("Initial schedule (10 tasks registered)", firstPlan);

        registerAdditionalTasks(registry, trace);

        const auto combinedPlan = scheduler.buildSchedule(settings);
        validatePlanAgainstRegistry(registry, combinedPlan, 20);
        printPlan("Updated schedule (20 tasks registered)", combinedPlan);

        scheduler.execute(combinedPlan,
                          moo::ExecutionSettings{
                              .runFor = 10s,
                              .maxCpu = 100.0,
                              .maxMemory = 100.0,
                              .schedulerTick = 25ms,
                          });

        if (trace.entries.empty()) {
            throw std::runtime_error("Expected at least one task execution during scheduler run");
        }

        std::cout << "executions observed=" << trace.entries.size() << '\n';
        std::cout << "incremental_schedule_test passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& ex) {
        std::cerr << "incremental_schedule_test failed: " << ex.what() << '\n';
        return EXIT_FAILURE;
    }
}
