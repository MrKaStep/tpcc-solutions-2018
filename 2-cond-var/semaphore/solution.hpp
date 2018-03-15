#pragma once

#include <tpcc/stdlike/condition_variable.hpp>

namespace tpcc {
namespace solutions {

class Semaphore {
 public:
  explicit Semaphore(const size_t count = 0)
      : count_(count),
        num_waiting_(0) {
    // to be implemented
  }

  void Acquire() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (count_ == 0) {
      ++num_waiting_;
      has_tokens_.wait(lock, [this]() {
        return this->count_ > 0;
      });
      --num_waiting_;
    }
    --count_;
  }

  void Release() {
    std::unique_lock<std::mutex> lock(mutex_);
    ++count_;
    if (num_waiting_ > 0) {
      has_tokens_.notify_one();
    }
  }

 private:
  size_t count_;
  size_t num_waiting_;
  std::mutex mutex_;
  tpcc::condition_variable has_tokens_;
};

} // namespace solutions
} // namespace tpcc
