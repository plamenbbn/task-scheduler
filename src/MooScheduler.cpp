#include "moo/MooScheduler.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>

namespace moo {

namespace {

double benefitCostRatio(const CandidateSolution& candidate) {
    const double cost = candidate.objectives.cpuCost +
                        candidate.objectives.memoryCost +
                        candidate.objectives.networkCost + 1e-9;
    return candidate.objectives.missionBenefit / cost;
}

}  // namespace

MooScheduler::MooScheduler(const TaskRegistry& registry)
    : registry_(registry), rng_(std::random_device{}()) {}

SchedulePlan MooScheduler::buildSchedule(const OptimizationSettings& settings) {
    std::vector<TaskSnapshot> snapshots;
    const auto& tasks = registry_.tasks();
    snapshots.reserve(tasks.size());

    for (TaskId id = 0; id < tasks.size(); ++id) {
        const auto& task = tasks[id];
        snapshots.push_back(TaskSnapshot{
            .id = id,
            .missionBenefit = task->missionBenefit(),
            .costs = task->costs(),
            .mode = task->mode(),
        });
    }

    if (snapshots.empty()) {
        return {};
    }

    NSGA2 optimizer{settings.populationSize,
                    settings.generations,
                    settings.crossoverProbability,
                    settings.mutationProbability};

    auto population = optimizer.optimize(snapshots, rng_);
    if (population.empty()) {
        return {};
    }

    const auto bestIter = std::max_element(population.begin(), population.end(), [](const auto& a, const auto& b) {
        return benefitCostRatio(a) < benefitCostRatio(b);
    });

    SchedulePlan plan;
    plan.summary = bestIter->objectives;

    for (std::size_t idx = 0; idx < bestIter->decision.genes.size(); ++idx) {
        if (bestIter->decision.genes[idx] == 0) {
            continue;
        }
        const auto& task = tasks[idx];
        if (task->mode() == ExecutionMode::OneShot) {
            plan.oneShotTasks.push_back(task);
        } else {
            plan.daemonTasks.push_back(task);
        }
    }

    return plan;
}

void MooScheduler::execute(const SchedulePlan& plan) {
    for (const auto& task : plan.oneShotTasks) {
        task->run();
    }

    for (const auto& task : plan.daemonTasks) {
        std::jthread daemonThread([task]() {
            task->run();
        });
        daemonThread.detach();
    }
}

}  // namespace moo
