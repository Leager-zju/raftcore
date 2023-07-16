#pragma once

#include <functional>

namespace raftcore {
using func = std::function<void()>;
class Defer {
 public:
  Defer(func&& callback) { callback_ = std::move(callback); }
  ~Defer() { callback_(); }

 private:
  func callback_;
};

#define DEFER2(x, y) x##y
#define DEFER1(x, y) DEFER2(x, y)
#define DEFER0(x) DEFER1(x, __COUNTER__)
#define DEFER(expr) auto DEFER0(_defer) = Defer([&, this]() { expr; })
}  // namespace raftcore