#include <afina/concurrency/Executor.h>

namespace Afina {
namespace Concurrency {

Executor::Executor(int low_watermark, int high_watermark, int max_queue_size, int idle_time) :
  low_watermark(low_watermark),
  high_watermark(high_watermark),
  max_queue_size(max_queue_size),
  idle_time(idle_time),
  threads(high_watermark),
  tasks(std::deque<std::function<void()>>(max_queue_size)),
  state(State::kRun) {
    for(int i = 0; i < low_watermark; ++i) {
      threads.push_back(std::thread(&perform, this));
    }
  }

Executor::~Executor() {
  Stop();
}

/**
 * Signal thread pool to stop, it will stop accepting new jobs and close threads just after each become
 * free. All enqueued jobs will be complete.
 *
 * In case if await flag is true, call won't return until all background jobs are done and all threads are stopped
 */
void Executor::Stop(bool await) {
  if(state == State::kStopped)
    return;

  {
    std::unique_lock<std::mutex> lock(mutex);
    state = State::kStopping;
  }

  empty_condition.notify_all();

  for(std::thread &every_thread : threads) {
    every_thread.join();
  }

  threads.empty();
  state = State::kStopped;
}

/**
 * Main function that all pool threads are running. It polls internal task queue and execute tasks
 */
void perform(Executor *executor) {
  std::function<void()> task;
  while(executor->state == Executor::State::kRun) {
    {
      std::unique_lock<std::mutex> lock(executor->mutex);

      while (executor->tasks.empty() && executor->state == Executor::State::kRun) {
        if (executor->empty_condition.wait_for(lock, std::chrono::milliseconds(executor->idle_time))
              == std::cv_status::timeout) {
          executor->empty_condition.wait(lock);
        }
      }

      task = executor->tasks.front();
      executor->tasks.pop_front();
    }
    task();
  }
}

} // namespace Concurrency
} // namespace Afina
