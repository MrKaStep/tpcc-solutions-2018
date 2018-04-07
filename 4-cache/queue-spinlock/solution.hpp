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
      // to be continued
    }

    void ReleaseLock() {
      /* transfer ownership to the next guard node in spinlock wait queue
       * or reset tail pointer if there are no other contenders
       */

      // not implemented
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
