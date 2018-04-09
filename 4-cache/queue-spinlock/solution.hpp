#pragma once

#include <tpcc/stdlike/atomic.hpp>
#include <tpcc/concurrency/backoff.hpp>

namespace tpcc {
namespace solutions {

class QueueSpinLock {
 public:
  class LockGuard {
   public:
    explicit LockGuard(QueueSpinLock& spinlock) : spinlock_(spinlock) {
      AcquireLock();
    }

    ~LockGuard() {
      ReleaseLock();
    }

   private:
    void AcquireLock() {
      // add self to spinlock queue and wait for ownership

      LockGuard* prev_tail = spinlock_.wait_queue_tail_.exchange(this);
      if (!prev_tail) {
        return;
      }

      prev_tail->next_.store(this);

      Backoff backoff{};
      while (!is_owner_.load()) {
        backoff();
      }
    }

    void ReleaseLock() {
      /* transfer ownership to the next guard node in spinlock wait queue
       * or reset tail pointer if there are no other contenders
       */

      LockGuard* expected = this;
      if (spinlock_.wait_queue_tail_.compare_exchange_strong(expected,
                                                             nullptr)) {
        return;
      }

      Backoff backoff{};
      while (!next_.load()) {
        backoff();
      }
      next_.load()->is_owner_.store(true);
    }

   private:
    QueueSpinLock& spinlock_;

    tpcc::atomic<bool> is_owner_{false};
    tpcc::atomic<LockGuard*> next_{nullptr};
  };

 private:
  // tail of intrusive list of LockGuards
  tpcc::atomic<LockGuard*> wait_queue_tail_{nullptr};
};

}  // namespace solutions
}  // namespace tpcc
