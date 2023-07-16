#pragma once

namespace raftcore {
class Buffer {
 public:
  friend class Persister;
  template <class T>
  void Decode(T& data);

 private:
  char* buffer;
};

class Persister {
 public:
  void Persist(const Buffer& buffer);
  char* Read();

 private:
  char* data;
};
}  // namespace raftcore