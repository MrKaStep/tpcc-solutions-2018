#pragma once

#include <tpcc/stdlike/atomic.hpp>
#include <tpcc/support/compiler.hpp>
#include <tpcc/lockfree/x86_64_atomic_stamped_pointer.hpp>

#include <utility>

namespace tpcc {
namespace solutions {

using namespace lockfree;

template <typename T>
class LockFreeStack {
  struct Node {
    T item_;
    tpcc::atomic<Node*> next_;

    Node(T item) : item_(std::move(item)) {
    }
  };

 public:
  LockFreeStack() {
  }

  ~LockFreeStack() {
    CleanStack(top_);
    CleanStack(pool_top_);
  }

  void Push(T item) {
    Node* node = CreateNode(std::move(item));
    AttachNode(node, top_);
  }

  bool Pop(T& item) {
    Node* node = DetachNode(top_);
    if (!node)
      return false;
    item = std::move(node->item_);
    AttachNode(node, pool_top_);
    return true;
  }

 private:
  using TopPtr = X86AtomicStampedPointer<Node>;

  void AttachNode(Node* node, TopPtr& top) {
    auto curr_top = top_.Load();
    do {
      node->next_ = curr_top.raw_ptr_;
    } while (!top.CompareAndSet(curr_top, node));
  }

  Node* DetachNode(TopPtr& top) {
    auto curr_top = top.Load();
    do {
      if (!curr_top) {
        return nullptr;
      }
    } while (!top.CompareAndSet(curr_top, curr_top->next_));
    return curr_top.raw_ptr_;
  }

  Node* CreateNode(T item) {
    Node* node = DetachNode(pool_top_);
    if (!node) {
      return new Node(std::move(item));
    }
    return node;
  }

  void CleanStack(TopPtr& top) {
    Node* node = top.LoadPointer();
    while (node) {
      Node* next = node->next_;
      delete node;
      node = next;
    }
  }

  TopPtr top_{nullptr, 0};
  TopPtr pool_top_{nullptr, 0};
};

}  // namespace solutions
}  // namespace tpcc
