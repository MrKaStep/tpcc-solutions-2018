#pragma once

#include <tpcc/stdlike/condition_variable.hpp>

#include <cstddef>
#include <deque>
#include <mutex>
#include <stdexcept>

namespace tpcc {
namespace solutions {

class QueueClosed : public std::runtime_error {
 public:
  QueueClosed()
      : std::runtime_error("Queue closed for Puts") {
  }
};

template <typename T, class Container = std::deque<T>>
class BlockingQueue {
 public:
  // capacity == 0 means queue is unbounded
  explicit BlockingQueue(const size_t capacity = 0)
      : capacity_(capacity),
        producers_waiting_(0),
        consumers_waiting_(0),
        closed_(false) {
  }

  // throws QueueClosed exception after Close
  void Put(T item) {
    std::unique_lock<std::mutex> lock(mutex_);
    ++producers_waiting_;
    can_put_.wait(lock, [this]() {
      if (closed_) {
        --producers_waiting_;
        throw QueueClosed();
      }
      return !IsFull();
    });
    --producers_waiting_;
    items_.push_back(std::move(item));
    NotifyConsumer();
  }

  // returns false iff queue is empty and closed
  bool Get(T& item) {
    std::unique_lock<std::mutex> lock(mutex_);
    ++consumers_waiting_;
    can_get_.wait(lock, [this]() {
      return !IsEmpty() || closed_;
    });
    --consumers_waiting_;
    if (IsEmpty() && closed_) {
      return false;
    }
    item = std::move(items_.front());
    items_.pop_front();
    NotifyProducer();
    return true;
  }

  // close queue for Puts
  void Close() {
    std::unique_lock<std::mutex> lock(mutex_);
    closed_ = true;
    if (producers_waiting_ > 0)
      can_put_.notify_all();
    if (consumers_waiting_ > 0)
      can_get_.notify_all();
  }

 private:
  // internal predicates for condition variables

  bool IsFull() const {
    return capacity_ != 0 && items_.size() == capacity_;
  }

  bool IsEmpty() const {
    return items_.empty();
  }

  // notify wrappers
  void NotifyProducer() {
    if (producers_waiting_ > 0) {
      can_put_.notify_one();
    }
  }

  void NotifyConsumer() {
    if (consumers_waiting_ > 0) {
      can_get_.notify_one();
    }
  }

 private:
  std::mutex mutex_;

  size_t capacity_;
  Container items_;

  tpcc::condition_variable can_put_;
  size_t producers_waiting_;
  tpcc::condition_variable can_get_;
  size_t consumers_waiting_;

  bool closed_;

  // use tpcc::condition_variable-s
};

} // namespace solutions
} // namespace tpcc
