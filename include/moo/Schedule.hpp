#pragma once

#include "moo/Task.hpp"

#include <memory>
#include <vector>

namespace moo {

struct ScheduleObjective {
    double missionBenefit = 0.0;
    double cpuCost = 0.0;
    double memoryCost = 0.0;
    double networkCost = 0.0;
};

struct SchedulePlan {
    std::vector<std::shared_ptr<Task>> oneShotTasks;
    std::vector<std::shared_ptr<Task>> daemonTasks;
    ScheduleObjective summary;
};

}  // namespace moo
