#include "threadpool.h"

#include <exception>
#include <memory>
#include <mutex>

#include "raft/defer.h"
#include "raft/logger.h"

namespace raftcore {
ThreadPool::ThreadPool(uint32_t thread_cnt) : over_(false) {
  threads_.reserve(thread_cnt);
  while (thread_cnt--) {
    threads_.emplace_back([this, thread_cnt]() {
      while (true) {
        std::shared_ptr<Task> new_task(nullptr);

        {
          std::unique_lock<std::mutex> lock(queue_mtx_);
          while (!over_ && task_queue_.empty()) {
            cond_.wait(lock);
          }

          if (over_ && task_queue_.empty()) {
            return;
          }

          new_task = task_queue_.front();
          task_queue_.pop();
        }

        if (new_task) {
          try {
            (*new_task)();
          } catch (std::exception& e) {
            LOG("%s in %s", e.what(), new_task->Description());
          }
        }
      }
    });
  }
}

void ThreadPool::Kill() {
  {
    std::unique_lock<std::mutex> lock(queue_mtx_);
    over_ = true;
  }
  cond_.notify_all();

  for (auto&& thread : threads_) {
    if (thread.joinable()) {
      thread.join();
    }
  }
}

void ThreadPool::AddTask(std::shared_ptr<Task> task) {
  std::unique_lock<std::mutex> lock(queue_mtx_);
  if (!over_) {
    task_queue_.push(task);
    cond_.notify_one();
  }
}
}  // namespace raftcore