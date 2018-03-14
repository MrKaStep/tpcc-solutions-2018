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
        enqueued_threads_(0),
        current_state_(kWaiting) {
  }

  void PassThrough() {
    std::unique_lock<std::mutex> lock(mutex_);
    AwaitEnqueue(lock);
    ++enqueued_threads_;
    if (enqueued_threads_ == num_threads_) {
      current_state_ = kPassing;
      --enqueued_threads_;
      cond_var_.notify_all();
      AwaitPassed(lock);
      current_state_ = kWaiting;
      cond_var_.notify_all();
    } else {
      AwaitPassing(lock);
      --enqueued_threads_;
      if (enqueued_threads_ == 0) {
        cond_var_.notify_all();
      }
    }
  }

 private:
  enum State {
    kWaiting,
    kPassing
  };

  void AwaitEnqueue(std::unique_lock<std::mutex>& lock) {
    cond_var_.wait(lock, [this]() {
      return this->current_state_ == kWaiting;
    });
  }

  void AwaitPassed(std::unique_lock<std::mutex>& lock) {
    cond_var_.wait(lock, [this]() {
      return this->enqueued_threads_ == 0;
    });
  }

  void AwaitPassing(std::unique_lock<std::mutex>& lock) {
    cond_var_.wait(lock, [this]() {
      return this->current_state_ == kPassing;
    });
  }

  size_t num_threads_;
  std::mutex mutex_;
  tpcc::condition_variable cond_var_;
  size_t enqueued_threads_;
  State current_state_;
};

} // namespace solutions
} // namespace tpcc
