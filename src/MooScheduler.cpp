#include "moo/MooScheduler.hpp"
#include "moo/ThreadPool.hpp"

#include <algorithm>
#include <chrono>
#include <future>
#include <thread>
#include <vector>

namespace moo {

namespace {

constexpr std::size_t kHardMaxThreadPoolSize = 8;

double taskPriority(const std::shared_ptr<Task>& task) {
    const double totalCost = task->cpuCost() + task->memoryCost() + task->networkCost() + 1e-9;
    return task->missionBenefit() / totalCost;
}

struct RunningTask {
    std::shared_ptr<Task> task;
    std::future<void> completion;
};

bool canDispatch(const Task& task,
                 double usedCpu,
                 double usedMemory,
                 std::size_t runningCount,
                 const ExecutionSettings& settings) {
    return runningCount < settings.maxThreads &&
           (usedCpu + task.cpuCost() <= settings.maxCpu) &&
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
                  ThreadPool& threadPool,
                  std::vector<RunningTask>& running,
                  double& usedCpu,
                  double& usedMemory) {
    usedCpu += task->cpuCost();
    usedMemory += task->memoryCost();
    running.push_back(RunningTask{
        .task = task,
        .completion = threadPool.submit([task]() {
            task->run();
        }),
    });
}

}  // namespace

MooScheduler::MooScheduler(const TaskRegistry& registry)
    : registry_(registry), rng_(std::random_device{}()) {}

SchedulePlan MooScheduler::buildSchedule(const OptimizationSettings& settings) {
    (void)settings;

    const auto& tasks = registry_.tasks();
    if (tasks.empty()) {
        return {};
    }

    SchedulePlan plan;

    for (const auto& task : tasks) {
        plan.summary.missionBenefit += task->missionBenefit();
        plan.summary.cpuCost += task->cpuCost();
        plan.summary.memoryCost += task->memoryCost();
        plan.summary.networkCost += task->networkCost();

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
    if (settings.runFor.count() <= 0 || settings.maxThreads == 0) {
        return;
    }

    const std::size_t effectiveMaxThreads = std::min<std::size_t>(settings.maxThreads, kHardMaxThreadPoolSize);
    ThreadPool threadPool{effectiveMaxThreads};

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
            if (canDispatch(**it, usedCpu, usedMemory, running.size(), ExecutionSettings{
                    .runFor = settings.runFor,
                    .maxCpu = settings.maxCpu,
                    .maxMemory = settings.maxMemory,
                    .maxThreads = effectiveMaxThreads,
                    .schedulerTick = settings.schedulerTick,
                })) {
                dispatchTask(*it, threadPool, running, usedCpu, usedMemory);
                it = pendingOneShots.erase(it);
            } else {
                ++it;
            }
        }

        const auto now = std::chrono::steady_clock::now();
        for (auto& daemon : daemonStates) {
            if (now >= daemon.nextEligible &&
                canDispatch(*daemon.task, usedCpu, usedMemory, running.size(), ExecutionSettings{
                    .runFor = settings.runFor,
                    .maxCpu = settings.maxCpu,
                    .maxMemory = settings.maxMemory,
                    .maxThreads = effectiveMaxThreads,
                    .schedulerTick = settings.schedulerTick,
                })) {
                dispatchTask(daemon.task, threadPool, running, usedCpu, usedMemory);
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
