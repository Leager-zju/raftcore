#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>

namespace raftcore {
class Task {
 public:
  Task(const std::function<void()>& func, const std::string& describ)
      : func_(func), describe_(describ) {}
  void operator()() { func_(); }
  const char* Description() { return describe_.c_str(); }

 private:
  std::function<void()> func_;
  std::string describe_;
};

class ThreadPool {
 public:
  ThreadPool(uint32_t thread_cnt);
  ~ThreadPool() = default;
  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;

  void Kill();
  void AddTask(std::shared_ptr<Task> task);

 private:
  bool over_;
  std::vector<std::thread> threads_;
  std::queue<std::shared_ptr<Task>> task_queue_;
  std::mutex queue_mtx_;
  std::condition_variable cond_;
};
}  // namespace raftcore