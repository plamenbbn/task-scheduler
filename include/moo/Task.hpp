#pragma once

#include <chrono>
#include <stdexcept>
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
         ExecutionMode mode,
         std::chrono::milliseconds duration,
         std::chrono::milliseconds daemonFrequency = std::chrono::milliseconds{0})
        : name_(std::move(name)),
          missionBenefit_(missionBenefit),
          costs_(costs),
          mode_(mode),
          duration_(duration),
          daemonFrequency_(daemonFrequency) {
        if (duration_.count() < 0) {
            throw std::invalid_argument{"Task duration must be non-negative"};
        }
        if (mode_ == ExecutionMode::Daemon && daemonFrequency_.count() <= 0) {
            throw std::invalid_argument{"Daemon tasks require a positive execution frequency"};
        }
        if (mode_ == ExecutionMode::OneShot && daemonFrequency_.count() != 0) {
            throw std::invalid_argument{"One-shot tasks must not specify a daemon execution frequency"};
        }
    }

    virtual ~Task() = default;

    virtual void run() = 0;

    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    [[nodiscard]] double missionBenefit() const noexcept { return missionBenefit_; }
    [[nodiscard]] double cpuCost() const noexcept { return costs_.cpu; }
    [[nodiscard]] double memoryCost() const noexcept { return costs_.memory; }
    [[nodiscard]] double networkCost() const noexcept { return costs_.network; }
    [[nodiscard]] const TaskCosts& costs() const noexcept { return costs_; }
    [[nodiscard]] ExecutionMode mode() const noexcept { return mode_; }
    [[nodiscard]] std::chrono::milliseconds duration() const noexcept { return duration_; }
    [[nodiscard]] std::chrono::milliseconds daemonFrequency() const noexcept { return daemonFrequency_; }

private:
    std::string name_;
    double missionBenefit_;
    TaskCosts costs_;
    ExecutionMode mode_;
    std::chrono::milliseconds duration_;
    std::chrono::milliseconds daemonFrequency_;
};

}  // namespace moo
