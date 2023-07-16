#include "raft.h"

#include <boost/asio/io_service.hpp>
#include <boost/system/error_code.hpp>
#include <cassert>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <vector>

#include "logger.h"
#include "raft/common.h"
#include "raft/config.h"
#include "raft/defer.h"
#include "raft/messagechannel.h"
#include "raft/threadpool.h"

namespace raftcore {
using timer_callback = std::function<void(const boost::system::error_code &)>;

Raft::Raft(PeerId me, Config *config, std::shared_ptr<ThreadPool> thread_pool,
           std::shared_ptr<Channel<Entry>> apply_chan,
           std::shared_ptr<Persister> persister)
    : over_(false),
      thread_pool_(thread_pool),
      me_(me),
      state_(RaftState::Follower),
      config_(config),
      apply_chan_(apply_chan),
      persister_(persister) {
  entries_.emplace_back();
  // ReadPersist();
  election_service_ = new io_service;
  heartbeat_service_ = new io_service;

  election_timer_ = new Timer(*election_service_);
  heartbeat_timer_ = new Timer(*heartbeat_service_);
}

Raft::~Raft() {
  delete election_service_;
  delete heartbeat_service_;
  delete election_timer_;
  delete heartbeat_timer_;
}

bool Raft::SendMessage(pMessage msg) {
  return !Killed() && config_->GetOutputChan().Push(msg);
}

uint64_t Raft::Quorum() { return config_->NumOfPeers() / 2; }

void Raft::Run() {
  LOG("RaftNode %lu Run", me_);
  thread_pool_->AddTask(std::make_shared<Task>(
      [this]() {
        if (!Killed()) {
          ResetElectionTimer();
        }
      },
      "ResetElectionTimer"));
  thread_pool_->AddTask(std::make_shared<Task>(
      [this]() {
        if (!Killed()) {
          ResetHeartbeatTimer();
        }
      },
      "ResetHeartbeatTimer"));
  thread_pool_->AddTask(std::make_shared<Task>(
      [this]() {
        if (!Killed()) {
          Applier();
        }
      },
      "Applier"));
  thread_pool_->AddTask(std::make_shared<Task>(
      [this]() {
        if (!Killed()) {
          Receiver();
        }
      },
      "Receiver"));
}

void Raft::Start(std::string &&data, Index &log_index, bool &is_leader) {
  std::unique_lock<std::shared_mutex> lock(mu_);
  if (state_ != RaftState::Leader) {
    log_index = INVALID_INDEX;
    is_leader = false;
    return;
  }
  LOG("START %s", data.c_str());

  log_index = GetLastLogIndex() + 1;
  is_leader = true;
  match_index_[me_] = log_index;
  next_index_[me_] = log_index + 1;

  entries_.emplace_back(current_term_, log_index, std::move(data));
  Persist();
  StartHeartBeat();
}

void Raft::Persist() {
  // TODO:store current_term, vote_for, entries
  
  // Buffer buffer;
  // buffer.encode(current_term_);
  // buffer.encode(vote_for_);
  // buffer.encode(entries_);
  // persister_->Persist(buffer);
}

void Raft::ReadPersist(const char *data) {
  // TODO:load current_term, vote_for, entries
}

void Raft::MakeRequestVoteArgs(pMessage request, PeerId to) {
  request->type = RequestVoteArgsType;
  request->from = me_;
  request->to = to;
  request->from_term = current_term_;
  request->fuck_log_term = GetLastLogTerm();
  request->fuck_log_index = GetLastLogIndex();
}

void Raft::MakeAppendEntriesArgs(pMessage request, PeerId to) {
  request->type = AppendEntriesArgsType;
  request->from = me_;
  request->to = to;
  request->from_term = current_term_;
  request->leader_committed = leader_committed_;

  Index next_index = next_index_[to];
  request->fuck_log_term = GetTermAt(next_index - 1);
  request->fuck_log_index = next_index - 1;

  for (Index idx = next_index; idx < entries_.size(); idx++) {
    request->entries.emplace_back(GetTermAt(idx), idx, entries_[idx].data);
  }
}

void Raft::ResetElectionTimer() {
  election_timer_->cancel();
  timer_callback election_timer_call_back =
      [&, this](const boost::system::error_code &e) {
        if (e != boost::asio::error::operation_aborted && !Killed()) {
          std::unique_lock<std::shared_mutex> lock(mu_);
          if (state_ != RaftState::Leader) {
            current_term_++;
            votefor_ = me_;
            current_granted_ = 0;
            ChangeState(RaftState::Candidate);
            StartElection();
          }
          election_timer_->expires_from_now(GetElectionTimeout());
          election_timer_->async_wait(election_timer_call_back);
        }
      };
  election_timer_->expires_from_now(GetElectionTimeout());
  election_timer_->async_wait(election_timer_call_back);
  election_service_->run();
}

void Raft::ResetHeartbeatTimer() {
  heartbeat_timer_->cancel();
  timer_callback heartbeat_timer_call_back =
      [&, this](const boost::system::error_code &e) {
        if (e != boost::asio::error::operation_aborted && !Killed()) {
          std::unique_lock<std::shared_mutex> lock(mu_);
          if (state_ == RaftState::Leader) {
            StartHeartBeat();
            heartbeat_timer_->expires_from_now(GetHeartbeatTimeout());
            heartbeat_timer_->async_wait(heartbeat_timer_call_back);
          }
        }
      };
  heartbeat_timer_->expires_from_now(GetHeartbeatTimeout());
  heartbeat_timer_->async_wait(heartbeat_timer_call_back);
  heartbeat_service_->run();
}

void Raft::StartElection() {
  // locked
  // assert(state_ != RaftState::Leader);
  LOG("%lu start election", me_);

  std::shared_ptr<uint64_t> granted(0);
  for (PeerId peer = 0; peer < config_->NumOfPeers(); peer++) {
    if (peer == me_) {
      continue;
    }

    SendRequestVote(peer);
  }
}

void Raft::SendRequestVote(PeerId peer) {
  auto request = std::make_shared<Message>();
  MakeRequestVoteArgs(request, peer);
  SendMessage(request);
  LOG("%lu send %s", me_, request->ToString().c_str());
}

bool Raft::RequestVote(pMessage request, pMessage reply) {
  std::unique_lock<std::shared_mutex> lock(mu_);
  reply->type = RequestVoteReplyType;
  reply->from = me_;
  reply->to = request->from;
  reply->to_term = request->from_term;
  if (request->from_term < current_term_) {
    reply->err = ErrOldTerm;
    reply->from_term = current_term_;
    return true;
  }

  if (request->from_term == current_term_ && votefor_ != INVALID_PEER &&
      votefor_ != request->from) {
    reply->err = ErrAlreadyVoted;
    reply->from_term = current_term_;
    return true;
  }

  if (request->from_term > current_term_) {
    ChangeState(RaftState::Follower);
    current_term_ = request->from_term;
    votefor_ = request->from;
    current_granted_ = 0;
  }

  reply->from_term = current_term_;
  if (request->fuck_log_term < GetLastLogTerm() ||
      (request->fuck_log_term == GetLastLogTerm() &&
       request->fuck_log_index < GetLastLogIndex())) {
    reply->err = ErrOldTerm;
    return true;
  }

  reply->err = OK;
  votefor_ = request->from;
  thread_pool_->AddTask(std::make_shared<Task>(
      [this]() {
        if (!Killed()) ResetElectionTimer();
      },
      "ResetElectionTimer"));
  return true;
}

void Raft::HandleRequestVoteReply(pMessage reply) {
  std::unique_lock<std::shared_mutex> lock(mu_);
  LOG("%lu handle %s", me_, reply->ToString().c_str());
  if (current_term_ == reply->to_term &&
      state_ == RaftState::Candidate) {  // avoid old-timed reply
    if (reply->from_term > current_term_) {
      ChangeState(RaftState::Follower);
      current_term_ = reply->from_term;
      votefor_ = INVALID_PEER;
      current_granted_ = 0;
    } else if (reply->err == OK) {
      current_granted_++;
      if (current_granted_ > Quorum()) {
        LOG("%lu => Leader with current grant %d, total peers %zu", me_,
            current_granted_, config_->NumOfPeers());
        ChangeState(RaftState::Leader);
        for (PeerId peer = 0; peer < config_->NumOfPeers(); peer++) {
          match_index_[peer] = 0;
          next_index_[peer] = GetLastLogIndex() + 1;
        }
        thread_pool_->AddTask(std::make_shared<Task>(
            [this]() {
              if (!Killed()) ResetHeartbeatTimer();
            },
            "ResetHeartbeatTimer"));
        StartHeartBeat();
      }
    }
  }
}

void Raft::StartHeartBeat() {
  // locked
  LOG("%lu start heartbeat", me_);
  for (PeerId peer = 0; peer < config_->NumOfPeers(); peer++) {
    if (peer == me_) {
      continue;
    }
    thread_pool_->AddTask(std::make_shared<Task>(
        [=]() {
          if (!Killed()) SendAppendEntries(peer);
        },
        "SendAppendEntries"));
  }
}

void Raft::SendAppendEntries(PeerId peer) {
  // unlocked
  std::shared_lock<std::shared_mutex> lock(mu_);
  if (state_ != RaftState::Leader) {
    return;
  }

  auto request = std::make_shared<Message>();
  MakeAppendEntriesArgs(request, peer);
  if (SendMessage(request)) {
    LOG("%lu send %s", me_, request->ToString().c_str());
  }
}

void Raft::HandleAppendEntriesReply(pMessage reply) {
  std::unique_lock<std::shared_mutex> lock(mu_);
  if (current_term_ == reply->to_term && state_ == RaftState::Leader) {
    if (reply->from_term > current_term_) {
      ChangeState(RaftState::Follower);
      current_term_ = reply->from_term;
      votefor_ = INVALID_PEER;
      current_granted_ = 0;
    } else if (reply->err == OK) {
      match_index_[reply->from] = reply->fuck_log_index;
      next_index_[reply->from] = reply->fuck_log_index + 1;
      if (Index N = FindN(); N != INVALID_INDEX) {
        LOG("N = %lu", N);
        leader_committed_ = N;
      }
    } else {
      next_index_[reply->from] = reply->fuck_log_index;
    }
  }
}

bool Raft::AppendEntries(pMessage request, pMessage reply) {
  std::unique_lock<std::shared_mutex> lock(mu_);
  reply->type = AppendEntriesReplyType;
  reply->from = me_;
  reply->to = request->from;
  reply->to_term = request->from_term;

  if (request->from_term < current_term_) {
    reply->err = ErrOldTerm;
    reply->from_term = current_term_;
    return true;
  }

  if (request->from_term > current_term_) {
    current_term_ = request->from_term;
    votefor_ = INVALID_PEER;
    current_granted_ = 0;
  }

  reply->from_term = current_term_;
  ChangeState(RaftState::Follower);
  thread_pool_->AddTask(std::make_shared<Task>(
      [this]() {
        if (!Killed()) {
          ResetElectionTimer();
        }
      },
      "ResetElectionTimer"));

  if (request->fuck_log_index < base_index_) {
    reply->err = ErrLogNotMatch;
    reply->fuck_log_term = entries_[0].term;
    reply->fuck_log_index = base_index_;
  }

  if (request->fuck_log_index - base_index_ >= entries_.size()) {
    reply->err = ErrLogNotMatch;
    reply->fuck_log_term = INVALID_TERM;
    reply->fuck_log_index = GetLastLogIndex() + 1;
  }

  if (Term conflict_term = GetTermAt(request->fuck_log_index);
      conflict_term != request->fuck_log_term) {
    reply->err = ErrLogNotMatch;
    reply->fuck_log_term = conflict_term;

    Index left = base_index_;
    Index right = request->fuck_log_index;
    while (left < right - 1) {
      Index mid = left + (right - left) / 2;
      Term t = GetTermAt(mid);
      if (t >= conflict_term) {
        right = mid;
      } else {
        left = mid + 1;
      }
    }

    reply->fuck_log_index = GetTermAt(left) == conflict_term ? left : right;
    return true;
  }

  auto idx = request->fuck_log_index - base_index_ + 1;
  for (auto &&entry : request->entries) {
    if (idx < entries_.size()) {
      entries_[idx++] = entry;
    } else {
      entries_.push_back(entry);
    }
  }
  reply->err = OK;
  reply->fuck_log_term = (GetLastLogTerm());
  reply->fuck_log_index = (GetLastLogIndex());

  if (request->leader_committed > leader_committed_) {
    leader_committed_ =
        fmin(request->leader_committed,
             request->entries[request->entries.size() - 1].index);
    apply_cond_.notify_one();
  }
  return true;
}

Index Raft::FindN() {
  // locked
  // If there exists an N such that N > commitIndex, a majority
  // of matchIndex[i] ≥ N, and log[N].term == currentTerm:
  // set commitIndex = N
  std::vector<Index> matches(config_->NumOfPeers());
  for (auto i = 0; i < matches.size(); i++) {
    matches[i] = match_index_[i];
  }
  std::sort(matches.begin(), matches.end());
  Index N = matches[matches.size() / 2];

  while (N > leader_committed_) {
    if (entries_[N - base_index_].term == current_term_) {
      break;
    }
  }

  return N <= leader_committed_ ? INVALID_INDEX : N;
}

void Raft::Applier() {
  while (true) {
    EntryVec need_applied;
    {
      std::unique_lock<std::shared_mutex> lock(mu_);
      while (!Killed() && last_applied_ >= leader_committed_) {
        apply_cond_.wait(lock);
      }

      if (Killed() && last_applied_ >= leader_committed_) {
        return;
      }

      Index idx = last_applied_;

      for (; idx < leader_committed_; idx++) {
        need_applied.push_back(entries_[idx - base_index_]);
      }
    }

    for (auto entry : need_applied) {
      apply_chan_->Write(entry);
    }

    {
      std::unique_lock<std::shared_mutex> lock(mu_);
      last_applied_ = leader_committed_;
    }
  }
}

void Raft::Receiver() {
  while (!Killed()) {
    pMessage new_message;
    if (!config_->GetInputChan(me_).Pop(new_message)) {
      continue;
    }
    // LOG("%lu Get New msg %s", me_, new_message->ToString().c_str());

    switch (new_message->type) {
      case RequestVoteArgsType: {
        thread_pool_->AddTask(std::make_shared<Task>(
            [=]() {
              if (!Killed()) {
                pMessage reply = std::make_shared<Message>();
                RequestVote(new_message, reply);
                if (SendMessage(reply)) {
                  LOG("%lu Send %s", me_, reply->ToString().c_str());
                }
              }
            },
            "RequestVote"));
      } break;
      case RequestVoteReplyType: {
        thread_pool_->AddTask(std::make_shared<Task>(
            [=]() {
              if (!Killed()) {
                HandleRequestVoteReply(new_message);
              }
            },
            "HandleRequestVoteReply"));
      } break;
      case AppendEntriesArgsType: {
        thread_pool_->AddTask(std::make_shared<Task>(
            [=]() {
              if (!Killed()) {
                pMessage reply = std::make_shared<Message>();
                AppendEntries(new_message, reply);
                if (SendMessage(reply)) {
                  LOG("%lu Send %s", me_, reply->ToString().c_str());
                }
              }
            },
            "AppendEntries"));

      } break;
      case AppendEntriesReplyType: {
        thread_pool_->AddTask(std::make_shared<Task>(
            [=]() {
              if (!Killed()) {
                HandleAppendEntriesReply(new_message);
              }
            },
            "HandleAppendEntriesReply"));
      } break;
    }
  }
}

void Raft::Kill() {
  over_.store(true);
  apply_cond_.notify_one();
}

bool Raft::Killed() { return over_.load(); }
}  // namespace raftcore
