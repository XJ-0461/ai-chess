#pragma once

#include <cstddef>
#include <exception>
#include <iostream>
#include <string>
#include <utility>

#include <so_5/all.hpp>

#include "Application/Task/BlockingTask.hpp"

namespace chess::application::task {

// Number of worker threads in the shared pool.
inline constexpr std::size_t kWorkerThreadCount = 3;

// The pool worker. A single agent bound to an adv_thread_pool dispatcher: its
// BlockingTask handler is marked thread-safe, so the dispatcher runs jobs
// concurrently across the pool's threads, each job picked up by the next free
// thread (true "next available" dispatch - no manual round-robin needed, and
// no so5extra). Job failures are contained so one bad task cannot kill the pool.
class TaskExecutorWorker final : public so_5::agent_t {
public:
    TaskExecutorWorker(context_t ctx, so_5::mbox_t submission_mbox)
        : so_5::agent_t{std::move(ctx)},
          submission_mbox_{std::move(submission_mbox)} {}

    void so_define_agent() override {
        // thread_safe => the dispatcher may invoke this handler on several pool
        // threads at once. The handler must therefore touch no mutable agent
        // state without synchronisation; it only runs the captured job.
        so_subscribe(submission_mbox_).event(&TaskExecutorWorker::OnBlockingTask, so_5::thread_safe);
    }

private:
    void OnBlockingTask(mhood_t<BlockingTask> job) {
        if (!job->task) {
            return;
        }
        try {
            job->task();
        } catch (const std::exception& e) {
            std::cerr << "TaskExecutor worker: job threw: " << e.what() << "\n";
        } catch (...) {
            std::cerr << "TaskExecutor worker: job threw a non-std exception\n";
        }
    }

    so_5::mbox_t submission_mbox_;
};

// Introduces the shared TaskExecutor cooperation into the environment: a single
// worker agent fronting a fixed-size adv_thread_pool, listening on the public
// submission mbox. Returns that mbox. Call once during startup.
inline so_5::mbox_t IntroduceTaskExecutor(
    so_5::environment_t& env,
    const std::size_t worker_thread_count = kWorkerThreadCount
) {
    const so_5::mbox_t submission_mbox = env.create_mbox(std::string{kBlockingTaskMboxName});

    env.introduce_coop([&](so_5::coop_t& coop) {
        auto pool = so_5::disp::adv_thread_pool::make_dispatcher(env, worker_thread_count);
        coop.make_agent_with_binder<TaskExecutorWorker>(pool.binder(), submission_mbox);
    });

    return submission_mbox;
}

} // namespace chess::application::task
