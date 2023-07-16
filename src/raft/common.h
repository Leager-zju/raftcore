#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace raftcore {
using PeerId = uint64_t;
using Term = uint64_t;
using Index = uint64_t;

const PeerId INVALID_PEER = UINT64_MAX;
const Term INVALID_TERM = 0;
const Index INVALID_INDEX = 0;

enum class RaftState { Follower = 0, Candidate, Leader };

enum MessageType {
  RequestVoteArgsType,
  RequestVoteReplyType,
  AppendEntriesArgsType,
  AppendEntriesReplyType,
};

enum Err {
  OK = 0,
  ErrOldTerm,
  ErrAlreadyVoted,
  ErrLogNotMatch,
  ErrLogIndexOverFlow,
  ErrLogIndexUnderFlow
};

const char* TypeToString(MessageType type);

const char* ErrToString(Err err);

struct Entry {
  Entry(Term tm = INVALID_TERM, Index idx = INVALID_INDEX,
        const std::string& dt = "")
      : term(tm), index(idx), data(dt) {}

  uint64_t term;
  uint64_t index;
  std::string data;
};

struct Message {
  MessageType type;
  PeerId from;
  PeerId to;
  Term from_term;
  Term to_term;

  Term fuck_log_term;
  Index fuck_log_index;

  Index leader_committed;
  std::vector<Entry> entries;
  Err err;

  std::string ToString();
};

using pMessage = std::shared_ptr<Message>;
}  // namespace raftcore
