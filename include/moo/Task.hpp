#pragma once

#include <string>
#include <utility>

namespace moo {

enum class ExecutionMode {
    OneShot,
    Daemon
};

struct TaskCosts {
    double cpu;
    double memory;
    double network;
};

class Task {
public:
    Task(std::string name,
         double missionBenefit,
         TaskCosts costs,
         ExecutionMode mode) noexcept
        : name_(std::move(name)),
          missionBenefit_(missionBenefit),
          costs_(costs),
          mode_(mode) {}

    virtual ~Task() = default;

    virtual void run() = 0;

    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    [[nodiscard]] double missionBenefit() const noexcept { return missionBenefit_; }
    [[nodiscard]] double cpuCost() const noexcept { return costs_.cpu; }
    [[nodiscard]] double memoryCost() const noexcept { return costs_.memory; }
    [[nodiscard]] double networkCost() const noexcept { return costs_.network; }
    [[nodiscard]] const TaskCosts& costs() const noexcept { return costs_; }
    [[nodiscard]] ExecutionMode mode() const noexcept { return mode_; }

private:
    std::string name_;
    double missionBenefit_;
    TaskCosts costs_;
    ExecutionMode mode_;
};

}  // namespace moo
