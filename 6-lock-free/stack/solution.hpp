#pragma once

#include <tpcc/stdlike/atomic.hpp>
#include <tpcc/support/compiler.hpp>

#include <utility>

namespace tpcc {
namespace solutions {

// Treiber lock-free stack

template <typename T>
class LockFreeStack {
  struct Node {
    T item_;
    // to be continued

    Node(T item) : item_(std::move(item)) {
    }
  };

 public:
  LockFreeStack() {
  }

  ~LockFreeStack() {
    // not implemented
  }

  void Push(T item) {
    UNUSED(item);
    // not implemented
  }

  bool Pop(T& item) {
    UNUSED(item);
    return false;  // not implemented
  }

 private:
  tpcc::atomic<Node*> top_{nullptr};
};

}  // namespace solutions
}  // namespace tpcc
