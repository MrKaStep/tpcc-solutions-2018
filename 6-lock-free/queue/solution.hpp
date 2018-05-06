#pragma once

#include <tpcc/stdlike/atomic.hpp>
#include <tpcc/support/compiler.hpp>

#include <utility>

namespace tpcc {
namespace solutions {

// Michael-Scott lock-free queue

template <typename T>
class LockFreeQueue {
  struct Node {
    T item_;
    tpcc::atomic<Node*> next_{nullptr};

    Node() {
    }

    explicit Node(T item, Node* next = nullptr)
        : item_(std::move(item)), next_(next) {
    }
  };

 public:
  LockFreeQueue() {
    Node* dummy = new Node{};
    head_ = dummy;
    tail_ = dummy;
  }

  ~LockFreeQueue() {
    // not implemented
  }

  void Enqueue(T item) {
    UNUSED(item);
    // not implemented
  }

  bool Dequeue(T& item) {
    UNUSED(item);
    return false;  // not implemented
  }

 private:
  tpcc::atomic<Node*> head_{nullptr};
  tpcc::atomic<Node*> tail_{nullptr};
};

}  // namespace solutions
}  // namespace tpcc
