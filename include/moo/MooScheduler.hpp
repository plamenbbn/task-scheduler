#pragma once

#include "moo/Schedule.hpp"
#include "moo/TaskRegistry.hpp"
#include "moo/optimizer/NSGA2.hpp"

#include <random>

namespace moo {

struct OptimizationSettings {
    std::size_t populationSize = 48;
    std::size_t generations = 64;
    double crossoverProbability = 0.9;
    double mutationProbability = 0.1;
};

class MooScheduler {
public:
    explicit MooScheduler(const TaskRegistry& registry);

    SchedulePlan buildSchedule(const OptimizationSettings& settings);

    void execute(const SchedulePlan& plan);

private:
    const TaskRegistry& registry_;
    std::mt19937_64 rng_;
};

}  // namespace moo
