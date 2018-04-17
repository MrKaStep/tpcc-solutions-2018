#pragma once

#include <atomic>
#include <functional>
#include <mutex>

namespace tpcc {
namespace solutions {

template <typename T>
class LazyValue {
  using Factory = std::function<T*()>;

 public:
  explicit LazyValue(Factory create) : create_(create) {
  }

  T& Get() {
    // double checked locking pattern
    T* curr_ptr = ptr_to_value_.load(std::memory_order_acquire);
    if (curr_ptr == nullptr) {
      std::lock_guard<std::mutex> guard(mutex_);
      curr_ptr = ptr_to_value_.load(std::memory_order_relaxed);
      if (curr_ptr == nullptr) {
        curr_ptr = create_();
        ptr_to_value_.store(curr_ptr, std::memory_order_release);
      }
    }
    return *curr_ptr;
  }

  ~LazyValue() {
    if (ptr_to_value_.load() != nullptr) {
      delete ptr_to_value_;
    }
  }

 private:
  Factory create_;
  std::mutex mutex_;
  std::atomic<T*> ptr_to_value_{nullptr};
};

}  // namespace solutions
}  // namespace tpcc
