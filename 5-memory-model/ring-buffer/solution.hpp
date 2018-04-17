#pragma once

#include <atomic>
#include <vector>

namespace tpcc {
namespace solutions {

// Single-Producer/Single-Consumer Fixed-Size Ring Buffer (Queue)

template <typename T>
class SPSCRingBuffer {
 public:
  explicit SPSCRingBuffer(const size_t capacity) : buffer_(capacity + 1) {
  }

  bool Publish(T element) {
    const size_t curr_head = head_.load(std::memory_order_acquire);
    const size_t curr_tail = tail_.load(std::memory_order_relaxed);

    if (IsFull(curr_head, curr_tail)) {
      return false;
    }

      buffer_[curr_tail] = std::move(element);
    tail_.store(Next(curr_tail), std::memory_order_release);
    return true;
  }

  bool Consume(T& element) {
    const size_t curr_head = head_.load(std::memory_order_relaxed);
    const size_t curr_tail = tail_.load(std::memory_order_acquire);

    if (IsEmpty(curr_head, curr_tail)) {
      return false;
    }

    element = std::move(buffer_[curr_head]);
    head_.store(Next(curr_head), std::memory_order_release);
    return true;
  }

 private:
  bool IsFull(const size_t head, const size_t tail) const {
    return Next(tail) == head;
  }

  bool IsEmpty(const size_t head, const size_t tail) const {
    return tail == head;
  }

  size_t Next(const size_t slot) const {
    return (slot + 1) % buffer_.size();
  }

 private:
  std::vector<T> buffer_;
  std::atomic<size_t> tail_{0};
  std::atomic<size_t> head_{0};
};

}  // namespace solutions
}  // namespace tpcc
