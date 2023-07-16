#pragma once

#include <atomic>
#include <condition_variable>
#include <queue>
#include "common.h"

namespace raftcore {
class MessageChannel {
 public:
  MessageChannel(PeerId id = INVALID_PEER);
  MessageChannel(const MessageChannel& other);
  ~MessageChannel() = default;
  
  bool Push(pMessage input);
  bool Pop(pMessage& output);

  void Kill();

 private:
  PeerId id_;
  bool over_;
  std::mutex mu_;
  std::condition_variable cond_;
  std::shared_ptr<std::queue<pMessage>> queue_;
};
}  // namespace raftcore