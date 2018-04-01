#pragma once

#include <tpcc/stdlike/atomic.hpp>
#include <tpcc/stdlike/condition_variable.hpp>
#include <tpcc/stdlike/mutex.hpp>

#include <tpcc/support/compiler.hpp>

#include <algorithm>
#include <forward_list>
#include <functional>
#include <shared_mutex>
#include <vector>
#include <utility>

namespace tpcc {
namespace solutions {

////////////////////////////////////////////////////////////////////////////////

// implement writer-priority rwlock
class ReaderWriterLock {
 public:
  // reader section / shared ownership

  void lock_shared() {
    // to be implemented
  }

  void unlock_shared() {
    // to be implemented
  }

  // writer section / exclusive ownership

  void lock() {
    // to be implemented
  }

  void unlock() {
    // to be implemented
  }

 private:
  // use mutex / condition_variable from tpcc namespace
};

////////////////////////////////////////////////////////////////////////////////

template <typename T, class HashFunction = std::hash<T>>
class StripedHashSet {
 private:
  using RWLock = ReaderWriterLock;  // std::shared_timed_mutex

  using ReaderLocker = std::shared_lock<RWLock>;
  using WriterLocker = std::unique_lock<RWLock>;

  using Bucket = std::forward_list<T>;
  using Buckets = std::vector<Bucket>;

 public:
  explicit StripedHashSet(const size_t concurrency_level = 4,
                          const size_t growth_factor = 2,
                          const double max_load_factor = 0.8)
      : concurrency_level_(concurrency_level),
        growth_factor_(growth_factor),
        max_load_factor_(max_load_factor) {
  }

  bool Insert(T element) {
    UNUSED(element);
    return false;  // not implemented
  }

  bool Remove(const T& element) {
    UNUSED(element);
    return false;  // not implemented
  }

  bool Contains(const T& element) const {
    UNUSED(element);
    return false;  // not implemented
  }

  size_t GetSize() const {
    return 0;  // to be implemented
  }

  size_t GetBucketCount() const {
    // for testing purposes
    // do not optimize, just acquire arbitrary lock and read bucket count
    return 0;  // to be implemented
  }

 private:
  size_t GetStripeIndex(const size_t hash_value) const {
    UNUSED(hash_value);
    return 0;  // to be implemented
  }

  // use: auto stripe_lock = LockStripe<ReaderLocker>(hash_value);
  template <class Locker>
  Locker LockStripe(const size_t hash_value) const {
    UNUSED(stripe_index);
    // to be implemented
  }

  size_t GetBucketIndex(const size_t hash_value) const {
    UNUSED(hash_value);
    return 0;  // to be implemented
  }

  Bucket& GetBucket(const size_t hash_value) {
    UNUSED(hash_value);
    // to be implemented
  }

  const Bucket& GetBucket(const size_t hash_value) const {
    UNUSED(hash_value);
    // to be implemented
  }

  bool MaxLoadFactorExceeded() const {
    return false;  // to be implemented
  }

  void TryExpandTable(const size_t expected_bucket_count) {
    UNUSED(expected_bucket_count);
    // to be implemented
  }

 private:
  size_t concurrency_level_;
  size_t growth_factor_;
  double max_load_factor_;

  // to be continued
};

}  // namespace solutions
}  // namespace tpcc
