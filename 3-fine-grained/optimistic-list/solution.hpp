#pragma once

#include <tpcc/concurrency/backoff.hpp>
#include <tpcc/memory/bump_pointer_allocator.hpp>
#include <tpcc/stdlike/atomic.hpp>
#include <tpcc/support/compiler.hpp>

#include <limits>
#include <mutex>
#include <thread>

namespace tpcc {
namespace solutions {

////////////////////////////////////////////////////////////////////////////////

// implement Test-and-TAS (!) spinlock
class SpinLock {
 public:
  void Lock() {
    Backoff backoff;
    while (locked_.load() || locked_.exchange(true)) {
      backoff();
    }
  }

  void Unlock() {
    locked_.store(false);
  }

  // adapters for BasicLockable concept

  void lock() {
    Lock();
  }

  void unlock() {
    Unlock();
  }

 private:
  tpcc::atomic<bool> locked_{false};
};

////////////////////////////////////////////////////////////////////////////////

// don't touch this
template <typename T>
struct KeyTraits {
  static T LowerBound() {
    return std::numeric_limits<T>::min();
  }

  static T UpperBound() {
    return std::numeric_limits<T>::max();
  }
};

////////////////////////////////////////////////////////////////////////////////

template <typename T, class TTraits = KeyTraits<T>>
class OptimisticLinkedSet {
 private:
  struct Node {
    T key_;
    tpcc::atomic<Node*> next_;
    SpinLock spinlock_;
    bool marked_{false};

    Node(const T& key, Node* next = nullptr) : key_(key), next_(next) {
    }

    // use: auto node_lock = node->Lock();
    std::unique_lock<SpinLock> Lock() {
      return std::unique_lock<SpinLock>{spinlock_};
    }
  };

  struct EdgeCandidate {
    Node* pred_;
    Node* curr_;

    EdgeCandidate(Node* pred, Node* curr) : pred_(pred), curr_(curr) {
    }
  };

 public:
  explicit OptimisticLinkedSet(BumpPointerAllocator& allocator)
      : allocator_(allocator) {
    CreateEmptyList();
  }

  bool Insert(T key) {
    for (;;) {
      auto edge = Locate(key);
      auto pred_lock = edge.pred_->Lock();
      auto curr_lock = edge.curr_->Lock();
      if (!Validate(edge))
        continue;
      if (edge.curr_->key_ == key)
        return false;
      Node* new_node = allocator_.New<Node>(key, edge.curr_);
      edge.pred_->next_.store(new_node);
      size_.fetch_add(1);
      return true;
    }
  }

  bool Remove(const T& key) {
    for (;;) {
      auto edge = Locate(key);
      auto pred_lock = edge.pred_->Lock();
      auto curr_lock = edge.curr_->Lock();
      if (!Validate(edge))
        continue;
      if (edge.curr_->key_ != key)
        return false;
      edge.curr_->marked_ = true;
      edge.pred_->next_.store(edge.curr_->next_.load());
      size_.fetch_sub(1);
      return true;
    }
  }

  bool Contains(const T& key) const {
//    for (;;) {
      auto edge = Locate(key);
//      auto pred_lock = edge.pred_->Lock();
//      auto curr_lock = edge.curr_->Lock();
//      if (!Validate(edge))
//        continue;
      return edge.curr_->key_ == key;
//    }
  }

  size_t GetSize() const {
    return size_.load();
  }

 private:
  void CreateEmptyList() {
    // create sentinel nodes
    head_ = allocator_.New<Node>(TTraits::LowerBound());
    head_->next_ = allocator_.New<Node>(TTraits::UpperBound());
  }

  EdgeCandidate Locate(const T& key) const {
    Node* pred = head_;
    Node* curr = head_->next_;
    while (curr->key_ < key) {
      pred = curr;
      curr = curr->next_;
    }
    return {pred, curr};
  }

  bool Validate(const EdgeCandidate& edge) const {
    return !edge.pred_->marked_ && !edge.curr_->marked_ &&
           edge.pred_->next_ == edge.curr_;
  }

 private:
  BumpPointerAllocator& allocator_;
  Node* head_{nullptr};
  tpcc::atomic<size_t> size_{0};
};

}  // namespace solutions
}  // namespace tpcc
