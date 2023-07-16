#include "common.h"

#include <sstream>

namespace raftcore {

const char* TypeToString(MessageType type) {
  static const char* Str[4] = {"RequestVoteArgs", "RequestVoteReply",
                               "AppendEntriesArgs", "AppendEntriesReply"};

  return Str[type];
}

const char* ErrToString(Err err) {
  static const char* Str[6] = {"OK",
                               "ErrOldTerm",
                               "ErrAlreadyVoted",
                               "ErrLogNotMatch",
                               "ErrLogIndexOverFlow",
                               "ErrLogIndexUnderFlow"};

  return Str[err];
}

std::string Message::ToString() {
  std::ostringstream oss;
  oss << "{Type: " << TypeToString(type) << ", From " << from << ", To: " << to;

  switch (type) {
    case RequestVoteArgsType:
      oss << ", last log term: " << fuck_log_term
          << ", last log index: " << fuck_log_index;
      break;
    case RequestVoteReplyType:
      oss << ", grant: " << ErrToString(err);
      break;
    case AppendEntriesArgsType:
      oss << ", prev log term: " << fuck_log_term
          << ", prev log index: " << fuck_log_index
          << ", leader committed: " << leader_committed << ", Entries: ";
      for (auto&& entry : entries) {
        oss << "{data: " << entry.data << ", index: " << entry.index
            << ", term: " << entry.term << "}, ";
      }
      break;
    case AppendEntriesReplyType:
      oss << ", conflict log term: " << fuck_log_term
          << ", conflict log index: " << fuck_log_index
          << ", success: " << ErrToString(err);
      break;
  }
  oss << "}";
  return oss.str();
}

}  // namespace raftcore