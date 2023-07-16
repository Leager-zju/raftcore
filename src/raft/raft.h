#pragma once

#include <atomic>
#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/posix_time/posix_time_duration.hpp>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

#include "channel.h"
#include "common.h"
#include "defer.h"
#include "persister.h"
#include "raft/messagechannel.h"
#include "threadpool.h"

namespace raftcore {
class Config;
using EntryVec = std::vector<Entry>;
using IndexMap = std::unordered_map<PeerId, Index>;
using Timer = boost::asio::deadline_timer;

namespace io = boost::asio;
using io::io_service;

class Raft {
 public:
  Raft(PeerId me, Config *config, std::shared_ptr<ThreadPool> thread_pool,
       std::shared_ptr<Channel<Entry>> apply_chan,
       std::shared_ptr<Persister> persister);

  ~Raft();

  void ChangeState(RaftState target) { state_ = target; }

  boost::posix_time::milliseconds GetElectionTimeout() {
    return boost::posix_time::milliseconds(std::rand() % 1000 + 1000);
  }

  boost::posix_time::milliseconds GetHeartbeatTimeout() {
    return boost::posix_time::milliseconds(100);
  }

  Term GetLastLogTerm() { return entries_.back().term; }

  Index GetLastLogIndex() { return entries_.back().index; }

  Index GetLeaderCommited() {
    std::shared_lock<std::shared_mutex> lock(mu_);
    return leader_committed_;
  }

  Term GetTermAt(Index idx) {
    return (idx - base_index_ < entries_.size() && idx >= base_index_)
               ? entries_[idx - base_index_].term
               : INVALID_TERM;
  }

  void Kill();

  bool Killed();

  bool SendMessage(pMessage msg);

  uint64_t Quorum();

  void Run();

  void Start(std::string &&datadata, Index &log_index, bool &is_leader);

  void Persist();

  void ReadPersist(const char *data);

  void ResetElectionTimer();

  void ResetHeartbeatTimer();

  void StartElection();

  void MakeRequestVoteArgs(pMessage request, PeerId to);

  void SendRequestVote(PeerId peer);

  bool RequestVote(pMessage request, pMessage reply);

  void HandleRequestVoteReply(pMessage reply);

  void StartHeartBeat();

  void MakeAppendEntriesArgs(pMessage args, PeerId to);

  void SendAppendEntries(PeerId peer);

  bool AppendEntries(pMessage request, pMessage reply);

  void HandleAppendEntriesReply(pMessage reply);

  Index FindN();

  void Applier();

  void Receiver();

 private:
  std::shared_mutex mu_;
  std::atomic_bool over_;
  std::shared_ptr<ThreadPool> thread_pool_;

  PeerId me_ = INVALID_PEER;
  PeerId votefor_ = INVALID_PEER;
  uint16_t current_granted_ = 0;

  RaftState state_;
  Term current_term_ = 0;
  EntryVec entries_;
  Index leader_committed_ = 0;
  Index last_applied_ = 0;
  Index base_index_ = 0;

  Config *config_;
  IndexMap match_index_;
  IndexMap next_index_;

  io_service *election_service_;
  io_service *heartbeat_service_;
  Timer *election_timer_;
  Timer *heartbeat_timer_;

  std::condition_variable_any apply_cond_;
  std::shared_ptr<Channel<Entry>> apply_chan_;

  // Storage* storage_;
  std::shared_ptr<Persister> persister_;
};
}  // namespace raftcore
