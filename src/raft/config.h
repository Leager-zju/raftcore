#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <vector>

#include "raft/common.h"
#include "raft/messagechannel.h"
#include "raft/threadpool.h"
#include "raft/raft.h"

namespace raftcore {
using PeerVec = std::vector<std::unique_ptr<Raft>>;

class Config {
 public:
  Config(int cnt);

  size_t NumOfPeers() { return peers_.size(); }
  const PeerVec& GetPeers() { return peers_; }
  Raft* GetPeer(PeerId peer) { return peers_[peer].get(); }

  MessageChannel& GetInputChan(PeerId me) { return inputs_[me]; }
  MessageChannel& GetOutputChan() { return output_; }

  void AddPeer(std::unique_ptr<Raft> new_peer);

  void Run();
  void Kill();
  bool Killed();

  void MessageRouter();

 private:
  std::atomic_bool over_;
  std::shared_ptr<ThreadPool> thread_pool_;
  PeerVec peers_;

  std::vector<MessageChannel> inputs_;
  MessageChannel output_;
};
}  // namespace raftcore