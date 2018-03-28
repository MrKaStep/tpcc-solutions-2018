#pragma once

#include "futex_like.hpp"

#include <atomic.hpp>

#include <mutex>

namespace tpcc {
namespace solutions {

class AdaptiveLock {
 public:
  AdaptiveLock() : is_free_(1), futex_(is_free_), num_waiting_(0) {}

  void Lock() {
    size_t one{1};
    num_waiting_.fetch_add(1);
    while (!is_free_.compare_exchange_weak(one, 0)) {
      one = 1;
      futex_.Wait(0);
    }
    num_waiting_.fetch_sub(1);
  }

  void Unlock() {
    is_free_ = 1;
    if (num_waiting_.load() > 0) {
      futex_.WakeOne();
    }
  }

 private:
  tpcc::atomic<size_t> is_free_;
  FutexLike<size_t> futex_;
  tpcc::atomic<size_t> num_waiting_;
};

} // namespace solutions
} // namespace tpcc
