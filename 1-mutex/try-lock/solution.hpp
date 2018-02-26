#pragma once

#include <atomic.hpp>
#include <backoff.hpp>

namespace tpcc {
namespace solutions {

class TicketLock {
 public:
  // don't change this method
  void Lock() {
    const size_t this_thread_ticket = next_free_ticket_.fetch_add(1);

    Backoff backoff{};
    while (this_thread_ticket != owner_ticket_.load()) {
      backoff();
    }
  }

  bool TryLock() {
    size_t current_ownet_ticket = owner_ticket_.load();
    return next_free_ticket_.compare_exchange_weak(current_ownet_ticket, current_ownet_ticket + 1);
  }

  // don't change this method
  void Unlock() {
    owner_ticket_.store(owner_ticket_.load() + 1);
  }

 private:
  tpcc::atomic<size_t> next_free_ticket_{0};
  tpcc::atomic<size_t> owner_ticket_{0};
};

} // namespace solutions
} // namespace tpcc
