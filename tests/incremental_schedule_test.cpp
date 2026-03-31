#include "moo/MooScheduler.hpp"
#include "moo/TaskRegistry.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

class TestTask final : public moo::Task {
public:
    TestTask(std::string name,
             double missionBenefit,
             moo::TaskCosts costs,
             moo::ExecutionMode mode) noexcept
        : moo::Task(std::move(name), missionBenefit, costs, mode) {}

    void run() override {}
};

std::shared_ptr<TestTask> makeRandomTask(std::size_t index, std::mt19937_64& rng) {
    std::uniform_real_distribution<double> benefitDist(1.0, 100.0);
    std::uniform_real_distribution<double> costDist(0.1, 20.0);
    std::bernoulli_distribution modeDist(0.4);

    return std::make_shared<TestTask>(
        "task-" + std::to_string(index),
        benefitDist(rng),
        moo::TaskCosts{
            .cpu = costDist(rng),
            .memory = costDist(rng),
            .network = costDist(rng),
        },
        modeDist(rng) ? moo::ExecutionMode::Daemon : moo::ExecutionMode::OneShot);
}

std::vector<std::shared_ptr<moo::Task>> collectPlannedTasks(const moo::SchedulePlan& plan) {
    std::vector<std::shared_ptr<moo::Task>> tasks;
    tasks.reserve(plan.oneShotTasks.size() + plan.daemonTasks.size());
    tasks.insert(tasks.end(), plan.oneShotTasks.begin(), plan.oneShotTasks.end());
    tasks.insert(tasks.end(), plan.daemonTasks.begin(), plan.daemonTasks.end());
    return tasks;
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
        std::mt19937_64 rng(0xC0FFEEULL);
        moo::TaskRegistry registry;

        for (std::size_t i = 0; i < 10; ++i) {
            registry.registerTask(makeRandomTask(i, rng));
        }

        moo::MooScheduler scheduler(registry);
        const moo::OptimizationSettings settings{
            .populationSize = 64,
            .generations = 80,
            .crossoverProbability = 0.9,
            .mutationProbability = 0.12,
        };

        const auto firstPlan = scheduler.buildSchedule(settings);
        validatePlanAgainstRegistry(registry, firstPlan, 10);

        for (std::size_t i = 10; i < 20; ++i) {
            registry.registerTask(makeRandomTask(i, rng));
        }

        const auto combinedPlan = scheduler.buildSchedule(settings);
        validatePlanAgainstRegistry(registry, combinedPlan, 20);

        std::cout << "incremental_schedule_test passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& ex) {
        std::cerr << "incremental_schedule_test failed: " << ex.what() << '\n';
        return EXIT_FAILURE;
    }
}
