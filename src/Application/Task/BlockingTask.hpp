#pragma once

#include <functional>
#include <string_view>

namespace chess::application::task {

// Public mbox name that components submit BlockingTask jobs to. It is a named
// MPMC mbox, so it can be obtained anywhere via
// so_environment().create_mbox(std::string{kBlockingTaskMboxName}).
inline constexpr std::string_view kBlockingTaskMboxName = "TaskExecutor.BlockingTask";

// A unit of long-running / potentially blocking work submitted to the shared
// TaskExecutor pool. Any component can post one of these to the public
// submission mbox (kBlockingTaskMboxName) to have it run on a pool worker
// thread instead of on the caller's actor, keeping total thread usage bounded.
struct BlockingTask {
    std::function<void()> task{};
};

} // namespace chess::application::task
