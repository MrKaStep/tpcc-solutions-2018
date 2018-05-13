#pragma once

#include <tpcc/lockfree/atomic_marked_pointer.hpp>
#include <tpcc/memory/bump_pointer_allocator.hpp>
#include <tpcc/stdlike/atomic.hpp>
#include <tpcc/support/compiler.hpp>

#include <limits>
#include <thread>
#include <utility>

namespace tpcc {
namespace solutions {

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
class LockFreeLinkedSet {
 private:
  struct Node {
    T key_;
    tpcc::lockfree::AtomicMarkedPointer<Node> next_;

    Node(const T& key, Node* next = nullptr) : key_(key), next_(next) {
    }

    bool IsMarked() const {
      return next_.IsMarked();
    }
  };

  struct EdgeCandidate {
    Node* pred_;
    Node* curr_;

    EdgeCandidate(Node* pred, Node* curr) : pred_(pred), curr_(curr) {
    }
  };

 public:
  explicit LockFreeLinkedSet(BumpPointerAllocator& allocator)
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

 private:
  BumpPointerAllocator& allocator_;
  Node* head_{nullptr};
};

}  // namespace solutions
}  // namespace tpcc
