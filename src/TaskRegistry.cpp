#include "moo/TaskRegistry.hpp"

#include <stdexcept>

namespace moo {

TaskId TaskRegistry::registerTask(std::shared_ptr<Task> task) {
    if (!task) {
        throw std::invalid_argument{"TaskRegistry::registerTask received nullptr"};
    }
    std::scoped_lock lock{mutex_};
    tasks_.push_back(std::move(task));
    return tasks_.size() - 1;
}

}  // namespace moo
