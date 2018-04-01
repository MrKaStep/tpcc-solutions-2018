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

#include <iostream>

namespace tpcc {
namespace solutions {

////////////////////////////////////////////////////////////////////////////////

// implement writer-priority rwlock
class ReaderWriterLock {
 public:
  ReaderWriterLock()
      : writer_active_(false),
        writers_waiting_(0),
        readers_active_(0),
        readers_waiting_(0) {
  }

  void lock_shared() {
    std::unique_lock<std::mutex> lock(mutex_);
    ++readers_waiting_;
    readers_allowed_.wait(
        lock, [this]() { return !writer_active_ && writers_waiting_ == 0; });
    ++readers_active_;
    --readers_waiting_;
  }

  void unlock_shared() {
    std::unique_lock<std::mutex> lock(mutex_);
    --readers_active_;
    Notify();
  }

  // writer section / exclusive ownership

  void lock() {
    std::unique_lock<std::mutex> lock(mutex_);
    ++writers_waiting_;
    writers_allowed_.wait(
        lock, [this]() { return !writer_active_ && readers_active_ == 0; });
    --writers_waiting_;
    writer_active_ = true;
  }

  void unlock() {
    std::unique_lock<std::mutex> lock(mutex_);
    writer_active_ = false;
    Notify();
  }

 private:
  void Notify() {
    if (writers_waiting_ > 0) {
      if (readers_active_ == 0) {
        writers_allowed_.notify_one();
      }
    } else if (readers_waiting_ > 0) {
      readers_allowed_.notify_all();
    }
  }

  std::mutex mutex_;
  tpcc::condition_variable readers_allowed_;
  tpcc::condition_variable writers_allowed_;

  bool writer_active_;
  size_t writers_waiting_;

  size_t readers_active_;
  size_t readers_waiting_;
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
        max_load_factor_(max_load_factor),
        size_(0),
        stripe_locks_(concurrency_level_),
        buckets_(concurrency_level_) {
  }

  bool Insert(T element) {
    size_t hash_value = hash_function_(element);
    auto write_lock = LockStripe<WriterLocker>(hash_value);
    auto& bucket = GetBucket(hash_value);
    auto it = std::find(bucket.begin(), bucket.end(), element);
    if (it != bucket.end()) {
      return false;
    }
    bucket.emplace_front(element);
    size_.fetch_add(1);

    if (MaxLoadFactorExceeded()) {
      size_t expected_bucket_count = buckets_.size();
      write_lock.unlock();
      write_lock.release();
      TryExpandTable(expected_bucket_count);
    }

    return true;
  }

  bool Remove(const T& element) {
    size_t hash_value = hash_function_(element);
    auto write_lock = LockStripe<WriterLocker>(hash_value);
    auto& bucket = GetBucket(hash_value);
    auto it = std::find(bucket.begin(), bucket.end(), element);
    if (it == bucket.end()) {
      return false;
    }
    bucket.remove(element);
    size_.fetch_sub(1);
    return true;
  }

  bool Contains(const T& element) const {
    size_t hash_value = hash_function_(element);
    auto read_lock = LockStripe<ReaderLocker>(hash_value);
    auto& bucket = GetBucket(hash_value);
    return std::find(bucket.begin(), bucket.end(), element) != bucket.end();
  }

  size_t GetSize() const {
    return size_.load();
  }

  size_t GetBucketCount() const {
    auto stripe_lock = LockStripe<ReaderLocker>(0);
    return buckets_.size();
  }

 private:
  size_t GetStripeIndex(const size_t hash_value) const {
    return hash_value % concurrency_level_;
  }

  template <class Locker>
  Locker LockStripeByIndex(const size_t stripe_index) const {
    return Locker(stripe_locks_[stripe_index]);
  }

  // use: auto stripe_lock = LockStripe<ReaderLocker>(hash_value);
  template <class Locker>
  Locker LockStripe(const size_t hash_value) const {
    return LockStripeByIndex<Locker>(GetStripeIndex(hash_value));
  }

  size_t GetBucketIndex(const size_t hash_value) const {
    return hash_value % buckets_.size();
  }

  Bucket& GetBucket(const size_t hash_value) {
    return buckets_[GetBucketIndex(hash_value)];
  }

  const Bucket& GetBucket(const size_t hash_value) const {
    return buckets_[GetBucketIndex(hash_value)];
  }

  bool MaxLoadFactorExceeded() const {
    return static_cast<double>(size_.load()) >
           static_cast<double>(buckets_.size()) * max_load_factor_;
  }

  void TryExpandTable(const size_t expected_bucket_count) {
    std::vector<WriterLocker> locks;
    locks.reserve(concurrency_level_);
    locks.emplace_back(LockStripeByIndex<WriterLocker>(0));

    if (buckets_.size() != expected_bucket_count) {
      return;
    }

    for (size_t i = 1; i < concurrency_level_; ++i) {
      locks.emplace_back(LockStripeByIndex<WriterLocker>(i));
    }

    size_t new_buckets_count = buckets_.size() * growth_factor_;
    Buckets old_buckets(new_buckets_count);
    std::swap(buckets_, old_buckets);

    for (auto& bucket : old_buckets) {
      for (T& element : bucket) {
        size_t hash_value = hash_function_(element);
        GetBucket(hash_value).emplace_front(std::move(element));
      }
    }
  }

 private:
  const size_t concurrency_level_;
  const size_t growth_factor_;
  const double max_load_factor_;

  std::atomic<size_t> size_;

  mutable std::vector<RWLock> stripe_locks_;
  Buckets buckets_;

  HashFunction hash_function_;
};

}  // namespace solutions
}  // namespace tpcc
