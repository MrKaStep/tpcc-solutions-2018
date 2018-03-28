#pragma once

#include <tpcc/stdlike/condition_variable.hpp>

#include <cstddef>
#include <mutex>

#include <iostream>

namespace tpcc {
namespace solutions {

class CyclicBarrier {
 public:
  explicit CyclicBarrier(const size_t num_threads)
      : num_threads_(num_threads),
        mutex_(),
        cond_var_(),
        enqueued_threads_{0, 0},
        current_pass_(0) {
  }

  void PassThrough() {
    std::unique_lock<std::mutex> lock(mutex_);

    ++enqueued_threads_[current_pass_];

    if (enqueued_threads_[current_pass_] == num_threads_) {
      enqueued_threads_[current_pass_] = 0;
      current_pass_ ^= 1;
      cond_var_.notify_all();
    } else {
      AwaitPass(lock, current_pass_);
    }
  }

 private:

  void AwaitPass(std::unique_lock<std::mutex> &lock, size_t current_pass) {
    cond_var_.wait(lock, [this, i = current_pass]() {
      return this->current_pass_ != i;
    });
  }

  size_t num_threads_;
  std::mutex mutex_;
  tpcc::condition_variable cond_var_;
  size_t enqueued_threads_[2];
  size_t current_pass_;
};

} // namespace solutions
} // namespace tpcc
