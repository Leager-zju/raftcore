#include "messagechannel.h"

#include <memory>
#include <mutex>

#include "raft/common.h"
#include "raft/logger.h"

namespace raftcore {
MessageChannel::MessageChannel(PeerId id) : over_(false), id_(id) {
  queue_ = std::make_shared<std::queue<pMessage>>();
}

MessageChannel::MessageChannel(const MessageChannel& other) {
  over_ = other.over_;
  id_ = other.id_;
  queue_ = other.queue_;
}

bool MessageChannel::Push(pMessage input) {
  std::unique_lock<std::mutex> lock(mu_);
  if (!over_) {
    queue_->push(input);
    cond_.notify_one();
    return true;
  }
  return false;
}

bool MessageChannel::Pop(pMessage& output) {
  std::unique_lock<std::mutex> lock(mu_);
  while (!over_ && queue_->empty()) {
    cond_.wait(lock);
  }

  if (!over_ && !queue_->empty()) {
    output = queue_->front();
    queue_->pop();
    return true;
  }

  return false;
}

void MessageChannel::Kill() {
  std::unique_lock<std::mutex> lock(mu_);
  over_ = true;
  cond_.notify_all();
}

}  // namespace raftcore