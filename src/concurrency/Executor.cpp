#include <afina/concurrency/Executor.h>
#include <algorithm>

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
    // lazily init min amount of threads
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

  // wait until every task is taken from queue
  // and every thread finished execution
  if(await) {
    std::unique_lock<std::mutex> lock(mutex);
    finished_condition.wait(lock, [this](){ return tasks.empty() && (busy_threads == 0); });
  }

  for(std::thread &every_thread : threads) {
    every_thread.join();
  }

  state = State::kStopped;
}

/**
 * Main function that all pool threads are running. It polls internal task queue and execute tasks
 */
void perform(Executor *executor) {
  std::function<void()> task;
  // executor->state == Executor::State::kRun
  while(true) {
    {
      std::unique_lock<std::mutex> lock(executor->mutex);

      // wait for task
      // executor->tasks.empty() && executor->state == Executor::State::kRun
      while(true) {

        // thread is doing nothing
        // thus excess
        ++executor->excess_threads;
        // check for idle time
        if (executor->empty_condition.wait_for(lock, std::chrono::milliseconds(executor->idle_time))
              == std::cv_status::timeout) {
          // thread is idle and we have more than
          // min amount of threads, kill this one
          if(executor->threads.size() > executor->low_watermark)
            executor->kill_thread();
          // we have exactly min amount of threads
          // wait until new task is given
          else {
            executor->empty_condition.wait(lock);
            break;
          }
        }
        // task OR stop signal recieved
        // thread is not excess
        --executor->excess_threads;
      }
      // if we recieved a new task
      if(!executor->tasks.empty()) {
        // indicate that we are working on this task
        ++executor->busy_threads;
        task = executor->tasks.front();
        executor->tasks.pop_front();
      }
      // if we recieved stop signal
      // stop execution
      else {
        break;
      }
    }
    // process new task
    task();

    //signal that task was succesfully finished
    {
      std::unique_lock<std::mutex> lock(executor->mutex);
      // we are done working with this task
      --executor->busy_threads;
      executor->finished_condition.notify_one();
    }
  }
}

void Executor::kill_thread() {
  // find out which thread wants to be killed
  std::thread::id cur_thread_id = std::this_thread::get_id();
  auto iter = std::find_if(threads.begin(), threads.end(), [=](std::thread &t) { return (t.get_id() == cur_thread_id); });

  // kill it
  iter->detach();
  --excess_threads;
  threads.erase(iter);
}

} // namespace Concurrency
} // namespace Afina
