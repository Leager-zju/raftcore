#include "persister.h"
#include "buffer.h"

namespace raftcore {
  void Persister::Persist(const Buffer &buffer) {
    const char* data = buffer.buffer;
  }
}