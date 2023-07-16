#pragma once

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <queue>
#include <shared_mutex>

namespace raftcore {
template <class T, size_t N = 0>
class Channel {
  using value_type      = T;
  using value_ref       = T&;
  using const_value_ref = const T&;

 public:
  Channel()                          = default;
  Channel(const Channel&)            = default;
  Channel& operator=(const Channel&) = default;

  ~Channel() {
    Close();
  }

  void Write(const_value_ref value) {
    std::unique_lock<std::mutex> lock(mtx_);

    while (Full() && !Busy()) {
      write_cond_.wait(lock);

      if (IsClosed()) {
        read_cond_.notify_all();
        return;
      }
    }
    channel_queue_.push(value);
    read_cond_.notify_one();
  }

  bool Read(value_ref value) {
    std::unique_lock<std::mutex> lock(mtx_);

    wait_++;
    write_cond_.notify_one();
    while (Empty()) {
      read_cond_.wait(lock);

      if (IsClosed()) {
        write_cond_.notify_all();
        return false;
      }
    }
    value = channel_queue_.front();
    channel_queue_.pop();
    wait_--;

    return true;
  }

  bool Full() {
    return channel_queue_.size() == N;
  }

  bool Empty() {
    return channel_queue_.empty();
  }

  bool Busy() {
    return wait_ > 0;
  }

  void Close() {
    std::shared_lock<std::shared_mutex> lock(shared_mtx_);
    close_ = true;
  }

  bool IsClosed() {
    std::shared_lock<std::shared_mutex> lock(shared_mtx_);
    return close_;
  }

 private:
  std::mutex mtx_;
  std::condition_variable write_cond_;
  std::condition_variable read_cond_;
  std::queue<value_type> channel_queue_;

  size_t wait_ = 0;

  bool close_ = false;
  std::shared_mutex shared_mtx_;
};
}  // namespace raftcore