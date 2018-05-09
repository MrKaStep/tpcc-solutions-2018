#pragma once

#include <tpcc/stdlike/atomic.hpp>
#include <tpcc/support/compiler.hpp>
#include <tpcc/support/defer.hpp>

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
    trash_ = dummy;
  }

  ~LockFreeQueue() {
    DeleteNodes(trash_, tail_, true);
  }

  void Enqueue(T item) {
    tpcc::Defer defer([this]() { --active_threads_; });
    StartMethod();
    Node* new_node = new Node(item);
    Node* curr_tail;
    while (true) {
      curr_tail = tail_;
      if (!curr_tail->next_) {
        Node* expected = nullptr;
        if (curr_tail->next_.compare_exchange_strong(expected, new_node)) {
          break;
        }
      } else {
        Node* expected = curr_tail;
        tail_.compare_exchange_strong(expected, expected->next_);
      }
    }
    tail_.compare_exchange_weak(curr_tail, new_node);
  }

  bool Dequeue(T& item) {
    tpcc::Defer defer([this]() { --active_threads_; });
    StartMethod();
    while (true) {
      Node* curr_head = head_;
      Node* curr_tail = tail_;

      if (curr_head == curr_tail) {
        if (!curr_head->next_) {
          return false;
        } else {
          tail_.compare_exchange_strong(curr_tail, curr_tail->next_);
        }
      } else {
        if (head_.compare_exchange_strong(curr_head, curr_head->next_)) {
          item = std::move(curr_head->next_.load()->item_);
          return true;
        }
      }
    }
  }

 private:
  void StartMethod() {
    if (++active_threads_ != 0) {
      return;
    }
    Node* curr_head = head_;
    Node* curr_trash = trash_;
    DeleteNodes(curr_trash, curr_head);
    trash_ = curr_head;
  }

  void DeleteNodes(Node* from, Node* to, bool inclusive = false) {
    for (Node* it = from; it != to;) {
      Node* next = it->next_;
      delete it;
      it = next;
    }
    if (inclusive)
      delete to;
  }

  tpcc::atomic<size_t> active_threads_{0};
  tpcc::atomic<Node*> trash_{nullptr};
  tpcc::atomic<Node*> head_{nullptr};
  tpcc::atomic<Node*> tail_{nullptr};
};

}  // namespace solutions
}  // namespace tpcc
