#include <unistd.h>

#include <boost/asio/io_service.hpp>
#include <exception>
#include <memory>
#include <string>
#include <vector>

#include "grpcpp/completion_queue.h"
#include "gtest/gtest.h"
#include "raft/common.h"
#include "raft/config.h"
#include "raft/logger.h"
#include "raft/messagechannel.h"
#include "raft/persister.h"
#include "raft/raft.h"

const int NodeCnt = 5;
using namespace raftcore;

TEST(Raft, raft_test) {
  try {
    Config config(NodeCnt);
    config.Run();
    sleep(3);

    bool is_leader = false;
    Index index = INVALID_INDEX;
    Raft* leader = nullptr;
    for (auto&& node : config.GetPeers()) {
      try {
        node->Start("??", index, is_leader);
      } catch (std::exception &e) {
        LOG("%s", e.what());
      }

      if (is_leader) {
        leader = node.get();
        break;
      }
    }
    sleep(3);
    ASSERT_TRUE(is_leader);
    ASSERT_EQ(index, 1);
    ASSERT_NE(leader, nullptr);
    ASSERT_EQ(leader->GetLeaderCommited(), 1);

    
    config.Kill();
    LOG("END");
  } catch (std::exception& e) {
    LOG("%s", e.what());
  }
}