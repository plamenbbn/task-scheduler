#pragma once

#include "moo/Schedule.hpp"
#include "moo/TaskRegistry.hpp"
#include "moo/optimizer/NSGA2.hpp"

#include <chrono>
#include <random>

namespace moo {

struct OptimizationSettings {
    std::size_t populationSize = 48;
    std::size_t generations = 64;
    double crossoverProbability = 0.9;
    double mutationProbability = 0.1;
    double maxCpu = 100.0;
    double maxMemory = 100.0;
    std::chrono::milliseconds planningWindow{10'000};
};

struct ExecutionSettings {
    std::chrono::milliseconds runFor{10'000};
    double maxCpu = 100.0;
    double maxMemory = 100.0;
    std::size_t maxThreads = 8;
    std::chrono::milliseconds schedulerTick{25};
};

class MooScheduler {
public:
    explicit MooScheduler(const TaskRegistry& registry);

    SchedulePlan buildSchedule(const OptimizationSettings& settings);

    void execute(const SchedulePlan& plan, const ExecutionSettings& settings = {});

private:
    const TaskRegistry& registry_;
    std::mt19937_64 rng_;
};

}  // namespace moo
