#include "config.h"

#include <cassert>
#include <memory>

#include "raft.h"
#include "raft/common.h"
#include "raft/logger.h"
#include "raft/messagechannel.h"
#include "raft/threadpool.h"

namespace raftcore {
Config::Config(int cnt) : over_(false) {
  thread_pool_ = std::make_shared<ThreadPool>(50 * cnt);

  inputs_.reserve(cnt);
  for (PeerId id = 0; id < cnt; id++) {
    inputs_.emplace_back(id);
  }

  peers_.reserve(cnt);
  for (int i = 0; i < cnt; i++) {
    std::shared_ptr<Channel<Entry>> apply_chan;
    std::shared_ptr<Persister> persister;
    AddPeer(
        std::make_unique<Raft>(i, this, thread_pool_, apply_chan, persister));
  }
}

void Config::AddPeer(std::unique_ptr<Raft> new_peer) {
  peers_.push_back(std::move(new_peer));
}

void Config::Run() {
  for (auto&& peer : peers_) {
    thread_pool_->AddTask(
        std::make_shared<Task>([&, this]() { peer->Run(); }, "peerRun"));
  }

  thread_pool_->AddTask(
      std::make_shared<Task>([this]() { MessageRouter(); }, "MessageRouter"));
}

void Config::Kill() {
  over_.store(true);
  for (auto&& input : inputs_) {
    input.Kill();
  }
  output_.Kill();
  for (auto&& peer : peers_) {
    peer->Kill();
  }
  thread_pool_->Kill();
}

bool Config::Killed() { return over_.load(); }

void Config::MessageRouter() {
  while (!Killed()) {
    pMessage new_msg;
    if (!output_.Pop(new_msg)) {
      continue;
    }
    assert(new_msg->from != new_msg->to);
    if (!inputs_[new_msg->to].Push(new_msg)) {
      continue;
    }
    // LOG("Collect msg %s", new_msg->ToString().c_str());
  }
}
}  // namespace raftcore