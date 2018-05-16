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

    bool TryMark() {
      Node* ptr = next_.LoadPointer();
      return next_.TryMark(ptr);
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
    Node* node = allocator_.New<Node>(key);
    for (;;) {
      auto edge = Locate(key);
      if (edge.curr_->key_ == key) {
        return false;
      }
      node->next_.Store(edge.curr_);
      if (edge.pred_->next_.CompareAndSet({edge.curr_, false}, {node, false})) {
        ++size_;
        return true;
      }
    }
  }

  bool Remove(const T& key) {
    for (;;) {
      auto edge = Locate(key);
      if (edge.curr_->key_ != key) {
        return false;
      }
      if (!edge.curr_->TryMark()) {
        continue;
      }
      edge.pred_->next_.CompareAndSet({edge.curr_, false},
                                      {edge.curr_->next_.LoadPointer(), false});
      --size_;
      return true;
    }
  }

  bool Contains(const T& key) const {
    auto edge = Locate(key);
    // if node is reached -- Contains intersects with possible remove
    return edge.curr_->key_ == key;
  }

  size_t GetSize() const {
    return size_;
  }

 private:
  void CreateEmptyList() {
    // create sentinel nodes
    head_ = allocator_.New<Node>(TTraits::LowerBound());
    head_->next_ = allocator_.New<Node>(TTraits::UpperBound());
  }

  EdgeCandidate Locate(const T& key) const {
    for (;;) {
      Node* pred = head_;
      Node* curr = head_->next_.LoadPointer();
      while (true) {
        while (curr->IsMarked()) {
          HelpDeleting(pred, curr);
          curr = curr->next_.LoadPointer();
        }
        if (pred->IsMarked()) {
          break;
        }
        if (curr->key_ >= key) {
          return {pred, curr};
        }
        pred = curr;
        curr = pred->next_.LoadPointer();
      }
    }
  }

  bool HelpDeleting(Node* pred, Node* curr) const {
    return pred->next_.CompareAndSet({curr, false},
                                     {curr->next_.LoadPointer(), false});
  }

 private:
  BumpPointerAllocator& allocator_;
  Node* head_{nullptr};
  tpcc::atomic<size_t> size_{0};
};

}  // namespace solutions
}  // namespace tpcc
