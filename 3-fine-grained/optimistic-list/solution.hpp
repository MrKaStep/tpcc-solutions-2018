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

// implement Test-and-TAS spinlock
class SpinLock {
 public:
  void Lock() {
    // to be implemented
  }

  void Unlock() {
    // to be implemented
  }

  // adapters for BasicLockable concept

  void lock() {
    Lock();
  }

  void unlock() {
    Unlock();
  }

 private:
  // use tpcc::atomics
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
    Node* next_;
    SpinLock spinlock_;
    bool marked_{false};

    Node(const T& key, Node* next = nullptr)
        : key_(key), next_(next) {
    }

    // use: auto node_lock = node->Lock();
    std::unique_lock<SpinLock> Lock() {
      return std::unique_lock<SpinLock>{spinlock_};
    }
  };

  struct EdgeCandidate {
    Node* pred_;
    Node* curr_;

    EdgeCandidate(Node* pred, Node* curr)
        : pred_(pred), curr_(curr) {
    }
  };

 public:
  explicit OptimisticLinkedSet(BumpPointerAllocator& allocator)
      : allocator_(allocator) {
    CreateEmptyList();
  }

  bool Insert(T key) {
    UNUSED(key);
    return false;  // to be implemented
  }

  bool Remove(const T& key) {
    UNUSED(key);
    return false;  // to be implemented
  }

  bool Contains(const T& key) const {
    UNUSED(key);
    return false;  // to be implemented
  }

  size_t GetSize() const {
    return 0;  // to be implemented
  }

 private:
  void CreateEmptyList() {
    // create sentinel nodes
    head_ = allocator_.New<Node>(TTraits::LowerBound());
    head_->next_ = allocator_.New<Node>(TTraits::UpperBound());
  }

  EdgeCandidate Locate(const T& key) const {
    UNUSED(key);
    return {nullptr, nullptr};  // to be implemented
  }

  bool Validate(const EdgeCandidate& edge) const {
    UNUSED(edge);
    return false;  // to be implemented
  }

 private:
  BumpPointerAllocator& allocator_;
  Node* head_{nullptr};
};

}  // namespace solutions
}  // namespace tpcc
