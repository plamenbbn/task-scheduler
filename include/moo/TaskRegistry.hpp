#pragma once

#include "moo/Task.hpp"
#include "moo/Types.hpp"

#include <memory>
#include <mutex>
#include <vector>

namespace moo {

using TaskId = std::size_t;

class TaskRegistry {
public:
    TaskRegistry() = default;

    TaskId registerTask(std::shared_ptr<Task> task);

    [[nodiscard]] const std::vector<std::shared_ptr<Task>>& tasks() const noexcept { return tasks_; }

private:
    std::vector<std::shared_ptr<Task>> tasks_;
    mutable std::mutex mutex_;
};

}  // namespace moo
