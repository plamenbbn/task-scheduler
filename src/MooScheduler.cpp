#include "moo/MooScheduler.hpp"

#include <algorithm>
#include <chrono>
#include <future>
#include <iostream>
#include <limits>
#include <thread>
#include <vector>

namespace moo {

namespace {

double benefitCostRatio(const CandidateSolution& candidate) {
    const double cost = candidate.objectives.cpuCost +
                        candidate.objectives.memoryCost +
                        candidate.objectives.networkCost + 1e-9;
    return candidate.effectiveBenefit / cost;
}

bool isFeasible(const CandidateSolution& candidate) {
    return candidate.cpuViolation <= 1e-9 && candidate.memoryViolation <= 1e-9;
}

double totalViolation(const CandidateSolution& candidate) {
    return candidate.cpuViolation + candidate.memoryViolation;
}

double taskPriority(const std::shared_ptr<Task>& task) {
    const double totalCost = task->cpuCost() + task->memoryCost() + task->networkCost() + 1e-9;
    return task->missionBenefit() / totalCost;
}

struct RunningTask {
    std::shared_ptr<Task> task;
    std::future<void> completion;
    std::chrono::steady_clock::time_point startedAt;
};

bool canDispatch(const Task& task,
                 double usedCpu,
                 double usedMemory,
                 const ExecutionSettings& settings) {
    return (usedCpu + task.cpuCost() <= settings.maxCpu) &&
           (usedMemory + task.memoryCost() <= settings.maxMemory);
}

void reclaimFinishedTasks(std::vector<RunningTask>& running,
                          double& usedCpu,
                          double& usedMemory) {
    for (auto it = running.begin(); it != running.end();) {
        if (it->completion.wait_for(std::chrono::milliseconds{0}) == std::future_status::ready) {
            it->completion.get();
            usedCpu -= it->task->cpuCost();
            usedMemory -= it->task->memoryCost();
            it = running.erase(it);
        } else {
            ++it;
        }
    }
}

void dispatchTask(const std::shared_ptr<Task>& task,
                  std::vector<RunningTask>& running,
                  double& usedCpu,
                  double& usedMemory) {
    usedCpu += task->cpuCost();
    usedMemory += task->memoryCost();
    running.push_back(RunningTask{
        .task = task,
        .completion = std::async(std::launch::async, [task]() {
            task->run();
        }),
        .startedAt = std::chrono::steady_clock::now(),
    });
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
            .duration = task->duration(),
            .daemonFrequency = task->daemonFrequency(),
        });
    }

    if (snapshots.empty()) {
        return {};
    }

    NSGA2 optimizer{settings.populationSize,
                    settings.generations,
                    settings.crossoverProbability,
                    settings.mutationProbability,
                    NSGA2::Limits{
                        .maxCpu = settings.maxCpu,
                        .maxMemory = settings.maxMemory,
                        .runFor = settings.planningWindow,
                    }};

    auto population = optimizer.optimize(snapshots, rng_);
    if (population.empty()) {
        return {};
    }

    const auto bestIter = std::max_element(population.begin(), population.end(), [](const auto& a, const auto& b) {
        const bool aFeasible = isFeasible(a);
        const bool bFeasible = isFeasible(b);
        if (aFeasible != bFeasible) {
            return !aFeasible;
        }
        if (!aFeasible && !bFeasible) {
            return totalViolation(a) > totalViolation(b);
        }
        const double aScore = benefitCostRatio(a);
        const double bScore = benefitCostRatio(b);
        if (aScore == bScore) {
            return a.effectiveBenefit < b.effectiveBenefit;
        }
        return aScore < bScore;
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

    auto byPriority = [](const auto& lhs, const auto& rhs) {
        const double lhsPriority = taskPriority(lhs);
        const double rhsPriority = taskPriority(rhs);
        if (lhsPriority == rhsPriority) {
            return lhs->name() < rhs->name();
        }
        return lhsPriority > rhsPriority;
    };

    std::sort(plan.oneShotTasks.begin(), plan.oneShotTasks.end(), byPriority);
    std::sort(plan.daemonTasks.begin(), plan.daemonTasks.end(), byPriority);

    return plan;
}

void MooScheduler::execute(const SchedulePlan& plan, const ExecutionSettings& settings) {
    if (settings.runFor.count() <= 0) {
        return;
    }

    const auto startedAt = std::chrono::steady_clock::now();
    const auto deadline = startedAt + settings.runFor;

    struct DaemonState {
        std::shared_ptr<Task> task;
        std::chrono::steady_clock::time_point nextEligible;
    };

    std::vector<RunningTask> running;
    running.reserve(plan.oneShotTasks.size() + plan.daemonTasks.size());

    std::vector<std::shared_ptr<Task>> pendingOneShots = plan.oneShotTasks;
    std::vector<DaemonState> daemonStates;
    daemonStates.reserve(plan.daemonTasks.size());
    for (const auto& task : plan.daemonTasks) {
        daemonStates.push_back(DaemonState{
            .task = task,
            .nextEligible = startedAt,
        });
    }

    double usedCpu = 0.0;
    double usedMemory = 0.0;

    while (std::chrono::steady_clock::now() < deadline) {
        reclaimFinishedTasks(running, usedCpu, usedMemory);

        for (auto it = pendingOneShots.begin(); it != pendingOneShots.end();) {
            if (canDispatch(**it, usedCpu, usedMemory, settings)) {
                dispatchTask(*it, running, usedCpu, usedMemory);
                it = pendingOneShots.erase(it);
            } else {
                ++it;
            }
        }

        const auto now = std::chrono::steady_clock::now();
        for (auto& daemon : daemonStates) {
            if (now >= daemon.nextEligible && canDispatch(*daemon.task, usedCpu, usedMemory, settings)) {
                dispatchTask(daemon.task, running, usedCpu, usedMemory);
                daemon.nextEligible = now + daemon.task->daemonFrequency();
            }
        }

        std::this_thread::sleep_for(settings.schedulerTick);
    }

    while (!running.empty()) {
        reclaimFinishedTasks(running, usedCpu, usedMemory);
        if (!running.empty()) {
            std::this_thread::sleep_for(settings.schedulerTick);
        }
    }
}

}  // namespace moo
