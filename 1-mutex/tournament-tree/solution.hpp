#pragma once

#include <atomic.hpp>
#include <support.hpp>
#include <backoff.hpp>

#include <vector>

namespace tpcc {
namespace solutions {

class PetersonLock {
 public:

  PetersonLock() : victim(0) {
    want[0].store(false);
    want[1].store(false);
  }

  void Lock(size_t thread_index) {
    Backoff backoff{};

    size_t other_index = 1 - thread_index;
    want[thread_index].store(true);
    victim.store(thread_index);
    while (want[other_index].load() && victim.load() == thread_index) {
      backoff();
    }
  }

  void Unlock(size_t thread_index) {
    want[thread_index].store(false);
  }

 private:
  std::array<tpcc::atomic<bool>, 2> want;
  tpcc::atomic<size_t> victim;
};

class TournamentTreeLock {
 public:
  explicit TournamentTreeLock(size_t num_threads)
      : locks_(num_threads == 0 ? 0 : num_threads - 1) {}

  void Lock(size_t thread_index) {
    size_t current_node = GetThreadLeaf(thread_index);
    while (current_node != 1) {
      LockParent(current_node);
      current_node = GetParent(current_node);
    }
  }

  void Unlock(size_t thread_index) {
    const size_t leaf_index = GetThreadLeaf(thread_index);
    size_t bit_index = 0;
    while ((1 << (bit_index + 1)) <= leaf_index) {
      ++bit_index;
    }
    size_t current_node = 1;
    while (current_node != leaf_index) {
      if (leaf_index & (1ul << (--bit_index)))
        current_node = GetRightChild(current_node);
      else
        current_node = GetLeftChild(current_node);
      UnlockParent(current_node);
    }
  }

 private:
  // Tree navigation

  size_t GetParent(size_t node_index) const {
    return node_index >> 1;
  }

  size_t GetLeftChild(size_t node_index) const {
    return (node_index << 1);
  }

  size_t GetRightChild(size_t node_index) const {
    return (node_index << 1) | 1;
  }

  size_t GetThreadLeaf(size_t thread_index) const {
    return thread_index + locks_.size() + 1;
  }

  void LockParent(size_t node_index) {
    locks_[GetParent(node_index) - 1].Lock(node_index & 1);
  }

  void UnlockParent(size_t node_index) {
    locks_[GetParent(node_index) - 1].Unlock(node_index & 1);
  }

 private:
  std::vector<PetersonLock> locks_;
};

} // namespace solutions
} // namespace tpcc
