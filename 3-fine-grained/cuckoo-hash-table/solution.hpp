#pragma once

#include <tpcc/concurrency/backoff.hpp>

#include <tpcc/stdlike/atomic.hpp>
#include <tpcc/stdlike/mutex.hpp>

#include <tpcc/logging/logging.hpp>

#include <tpcc/support/compiler.hpp>
#include <tpcc/support/random.hpp>

#include <algorithm>
#include <functional>
#include <set>
#include <utility>
#include <vector>

#include <cassert>

#include <iostream>

namespace tpcc {
namespace solutions {

////////////////////////////////////////////////////////////////////////////////

class TTASSpinLock {
 public:
  void lock() {
    Backoff backoff{};
    while (locked_.exchange(true)) {
      while (locked_.load()) {
        backoff();
      }
    }
  }

  void unlock() {
    locked_.store(false);
  }

 private:
  tpcc::atomic<bool> locked_{false};
};

////////////////////////////////////////////////////////////////////////////////

using Mutex = TTASSpinLock;
using MutexLocker = std::unique_lock<Mutex>;

class LockSet {
 public:
  LockSet& Lock(Mutex& mutex) {
    locks_.emplace_back(mutex);
    return *this;
  }

  void Unlock() {
    locks_.clear();
  }

 private:
  std::vector<MutexLocker> locks_;
};

class LockManager {
 public:
  LockManager(size_t concurrency_level) : locks_(concurrency_level) {
  }

  size_t GetLockCount() const {
    return locks_.size();
  }

  LockSet Lock(size_t i) {
    LockSet result;
    result.Lock(locks_[i]);
    return result;
  }

  LockSet Lock(size_t i, size_t j) {
    LockSet result;
    if (i > j) {
      std::swap(i, j);
    }
    result.Lock(locks_[i]);
    if (i != j) {
      result.Lock(locks_[j]);
    }
    return result;
  }

  LockSet Lock(std::vector<size_t> indices) {
    std::sort(indices.begin(), indices.end());
    LockSet result;
    for (size_t i : indices) {
      result.Lock(locks_[i]);
    }
    return result;
  }

  LockSet LockAllFrom(size_t start) {
    LockSet result;
    for (size_t i = start; i < locks_.size(); ++i) {
      result.Lock(locks_[i]);
    }
    return result;
  }

 private:
  std::vector<Mutex> locks_;
};

////////////////////////////////////////////////////////////////////////////////

struct ElementHash {
  size_t hash_value_;
  uint16_t alt_value_;

  bool operator==(const ElementHash& that) const {
    return hash_value_ == that.hash_value_;
  }

  bool operator!=(const ElementHash& that) const {
    return !(*this == that);
  }

  void Alternate() {
    hash_value_ ^= alt_value_;
  }
};

template <typename T, typename HashFunction>
class CuckooHasher {
 public:
  ElementHash operator()(const T& element) const {
    size_t hash_value = hasher_(element);
    return {.hash_value_ = hash_value,
            .alt_value_ = ComputeAltValue(hash_value)};
  }

  ElementHash AlternateHash(const ElementHash& hash) {
    return {.hash_value_ = hash.hash_value_ ^ hash.alt_value_,
            .alt_value_ = hash.alt_value_};
  }

 private:
  uint16_t ComputeAltValue(size_t hash_value) const {
    hash_value *= 1349110179037u;
    size_t alt_value = ((hash_value >> 48) ^ (hash_value >> 32) ^
                        (hash_value >> 16) ^ hash_value) &
                       0xFFFF;
    if ((alt_value & 1) == 0) {
      alt_value ^= 1;
    }

    return static_cast<uint16_t>(alt_value);
  }

 private:
  std::hash<T> hasher_;
};

////////////////////////////////////////////////////////////////////////////////

struct CuckooPathSlot {
  size_t bucket_;
  size_t slot_;
  // something else?

  CuckooPathSlot(size_t bucket, size_t slot) : bucket_(bucket), slot_(slot) {
  }
};

using CuckooPath = std::vector<CuckooPathSlot>;

////////////////////////////////////////////////////////////////////////////////

// use this exceptions to interrupt and retry current operation

struct TableOvercrowded : std::logic_error {
  TableOvercrowded() : std::logic_error("Table overcrowded") {
  }
};

struct TableExpanded : std::logic_error {
  TableExpanded() : std::logic_error("Table expanded") {
  }
};

////////////////////////////////////////////////////////////////////////////////

template <typename T, class HashFunction = std::hash<T>>
class CuckooHashSet {
 private:
  // tune thresholds
  const size_t kMaxPathLength = 10;
  const size_t kFindPathIterations = 16;
  const size_t kEvictIterations = 16;

 private:
  struct CuckooSlot {
    T element_;
    ElementHash hash_;
    bool occupied_{false};

    CuckooSlot() {
    }

    CuckooSlot(T element, const ElementHash& hash)
        : element_(std::move(element)), hash_(hash), occupied_(true) {
    }

    void Set(const T& element, ElementHash hash) {
      element_ = element;
      hash_ = hash;
      occupied_ = true;
    }

    void Clear() {
      occupied_ = false;
    }

    bool IsOccupied() const {
      return occupied_;
    }
  };

  using CuckooBucket = std::vector<CuckooSlot>;
  using CuckooBuckets = std::vector<CuckooBucket>;

  using TwoBuckets = std::pair<size_t, size_t>;
  using CuckooSlotRef = CuckooPathSlot;

 public:
  explicit CuckooHashSet(const size_t concurrency_level = 32,
                         const size_t bucket_width = 4)
      : lock_manager_(concurrency_level),
        concurrency_level_(concurrency_level),
        bucket_width_(bucket_width),
        bucket_count_(GetInitialBucketCount(concurrency_level)),
        buckets_(CreateEmptyTable(bucket_count_, bucket_width_)),
        size_(0) {
    expected_bucket_count_ = bucket_count_;
  }

  bool Insert(const T& element) {
    auto hash = hasher_(element);

    while (true) {
      RememberBucketCount();

      try {
        TwoBuckets buckets;
        LockSet bucket_locks;
        LockTwoBuckets(hash, buckets, bucket_locks);

        InterruptIfTableExpanded();

        size_t index;

        if (FindElement(element, buckets.first, index) ||
            FindElement(element, buckets.second, index)) {
          return false;
        }

        bool use_alternative_bucket = TossFairCoin();

        size_t bucket_index =
            use_alternative_bucket ? buckets.second : buckets.first;
        auto& bucket = buckets_[bucket_index];

        for (auto& slot : bucket) {
          if (!slot.IsOccupied()) {
            if (use_alternative_bucket)
              hash.Alternate();
            slot.Set(element, hash);
            size_.fetch_add(1);
            return true;
          }
        }

        bucket_locks.Unlock();
        EvictElement(bucket_index);
        LockTwoBuckets(hash, buckets, bucket_locks);

        InterruptIfTableExpanded();

        if (FindElement(element, buckets.first, index) ||
            FindElement(element, buckets.second, index)) {
          return false;
        }

        for (auto& slot : bucket) {
          if (!slot.IsOccupied()) {
            if (use_alternative_bucket)
              hash.Alternate();
            slot.Set(element, hash);
            size_.fetch_add(1);
            return true;
          }
        }
      } catch (const TableExpanded& _) {
        LOG_SIMPLE("Insert interrupted due to concurrent table expansion");
      } catch (const TableOvercrowded& _) {
        LOG_SIMPLE("Cannot insert element, table overcrowded");
        ExpandTable();
      }
    }
  }

  bool Remove(const T& element) {
    auto hash = hasher_(element);

    while (true) {
      RememberBucketCount();

      try {
        TwoBuckets buckets;
        LockSet bucket_locks;
        LockTwoBuckets(hash, buckets, bucket_locks);
        InterruptIfTableExpanded();

        size_t index;

        if (FindElement(element, buckets.first, index)) {
          buckets_[buckets.first][index].Clear();
          size_.fetch_sub(1);
          return true;
        } else if (FindElement(element, buckets.second, index)) {
          buckets_[buckets.second][index].Clear();
          size_.fetch_sub(1);
          return true;
        }

        return false;

      } catch (const TableExpanded& _) {
        LOG_SIMPLE("Remove interrupted due to concurrent table expansion");
      }
    }
  }

  bool Contains(const T& element) const {
    auto hash = hasher_(element);
    while (true) {
      RememberBucketCount();

      try {
        TwoBuckets buckets;
        LockSet bucket_locks;
        LockTwoBuckets(hash, buckets, bucket_locks);

        InterruptIfTableExpanded();

        size_t index;

        return FindElement(element, buckets.first, index) ||
               FindElement(element, buckets.second, index);

      } catch (const TableExpanded& _) {
        LOG_SIMPLE("Contains interrupted due to concurrent table expansion");
      }
    }
  }

  size_t GetSize() const {
    return size_.load();
  }

  double GetLoadFactor() const {
    return static_cast<double>(size_) /
           static_cast<double>(bucket_count_ * bucket_width_);
  }

 private:
  static CuckooBuckets CreateEmptyTable(const size_t bucket_count,
                                        const size_t bucket_width) {
    return CuckooBuckets(bucket_count, CuckooBucket(bucket_width));
  }

  static size_t GetInitialBucketCount(const size_t concurrency_level) {
    return std::max(concurrency_level << 1, 4ul);
  }

  static size_t HashToBucket(const size_t hash_value,
                             const size_t bucket_count) {
    return hash_value % bucket_count;
  }

  size_t HashToBucket(const size_t hash_value) const {
    return HashToBucket(hash_value, expected_bucket_count_);
  }

  size_t GetPrimaryBucket(const ElementHash& hash) const {
    return HashToBucket(hash.hash_value_);
  }

  size_t GetAlternativeBucket(const ElementHash& hash) const {
    size_t primary = hash.hash_value_;
    size_t alternative = hash.hash_value_ ^ hash.alt_value_;

    if (alternative % bucket_count_ == primary % bucket_count_) {
      return (primary ^ 1) % bucket_count_;
    }

    return alternative % bucket_count_;
  }

  TwoBuckets GetBuckets(const ElementHash& hash) const {
    return {GetPrimaryBucket(hash), GetAlternativeBucket(hash)};
  }

  void RememberBucketCount() const {
    // store current bucket count in thread local storage
    expected_bucket_count_ = bucket_count_.load();
  }

  void InterruptIfTableExpanded() const {
    if (bucket_count_.load() != expected_bucket_count_) {
      throw TableExpanded{};
    }
  }

  size_t GetLockIndex(const size_t bucket_index) const {
    return bucket_index % concurrency_level_;
  }

  LockSet LockTwoBuckets(const TwoBuckets& buckets) const {
    return lock_manager_.Lock(GetLockIndex(buckets.first),
                              GetLockIndex(buckets.second));
  }

  void LockTwoBuckets(const ElementHash& hash, TwoBuckets& buckets,
                      LockSet& locks) const {
    buckets = GetBuckets(hash);
    locks = LockTwoBuckets(buckets);
  }

  bool FindElement(const T& element, size_t bucket_index,
                   size_t& slot_index) const {
    for (size_t i = 0; i < bucket_width_; ++i) {
      auto& slot = buckets_[bucket_index][i];
      if (slot.IsOccupied() && slot.element_ == element) {
        slot_index = i;
        return true;
      }
    }
    return false;
  }

  // returns false if conflict detected during optimistic move
  bool TryMoveHoleBackward(const CuckooPath& path) {
    for (auto it = path.rbegin(); std::next(it) != path.rend(); ++it) {
      auto next_it = std::next(it);
      auto lock_set = lock_manager_.Lock(GetLockIndex(it->bucket_),
                                         GetLockIndex(next_it->bucket_));
      InterruptIfTableExpanded();

      auto& current_slot = buckets_[it->bucket_][it->slot_];
      auto& next_slot = buckets_[next_it->bucket_][next_it->slot_];

      if (!next_slot.IsOccupied()) {
        continue;
      }

      if (current_slot.IsOccupied() ||
          GetAlternativeBucket(next_slot.hash_) != it->bucket_) {
        return false;
      }

      next_slot.hash_.Alternate();
      std::swap(current_slot, next_slot);
    }
    return true;
  }

  bool TryFindCuckooPathWithRandomWalk(size_t start_bucket, CuckooPath& path) {
    for (size_t i = 0; i < kMaxPathLength; ++i) {
      auto lock_set = lock_manager_.Lock(GetLockIndex(start_bucket));

      InterruptIfTableExpanded();

      auto& bucket = buckets_[start_bucket];

      for (size_t j = 0; j < bucket_width_; ++j) {
        if (!bucket[j].IsOccupied()) {
          path.emplace_back(start_bucket, j);
          return true;
        }
      }

      size_t j = RandomUInteger(bucket_width_ - 1);

      path.emplace_back(start_bucket, j);
      start_bucket = GetAlternativeBucket(bucket[j].hash_);
    }
    path.clear();
    return false;
  }

  CuckooPath FindCuckooPath(size_t start_bucket) {
    CuckooPath path;
    for (size_t i = 0; i < kFindPathIterations; ++i) {
      if (TryFindCuckooPathWithRandomWalk(start_bucket, path)) {
        return path;
      }
    }

    throw TableOvercrowded{};
  }

  void EvictElement(size_t start_bucket) {
    for (size_t i = 0; i < kEvictIterations; ++i) {
      auto path = FindCuckooPath(start_bucket);
      if (TryMoveHoleBackward(path)) {
        return;
      }
    }

    throw TableOvercrowded{};
  }

  void ExpandTable() {
    auto initial_lock = lock_manager_.Lock(0);

    if (expected_bucket_count_ != buckets_.size())
      return;

    auto lock_set = lock_manager_.LockAllFrom(1);

    bucket_count_ = bucket_count_.load() * 2;
    expected_bucket_count_ = bucket_count_;

    CuckooBuckets new_buckets(bucket_count_);

    for (auto& bucket : new_buckets) {
      bucket.reserve(bucket_width_);
    }

    for (size_t i = 0; i < buckets_.size(); ++i) {
      auto& bucket = buckets_[i];
      for (size_t j = 0; j < bucket_width_; ++j) {
        auto& slot = bucket[j];
        if (slot.IsOccupied()) {
          size_t bucket_index = GetPrimaryBucket(slot.hash_);
          new_buckets[bucket_index].emplace_back(slot);
        }
      }
    }

    for (auto& bucket : new_buckets) {
      bucket.resize(bucket_width_);
    }

    buckets_ = std::move(new_buckets);
  }

 private:
  CuckooHasher<T, HashFunction> hasher_;

  mutable LockManager lock_manager_;

  const size_t concurrency_level_;
  const size_t bucket_width_;  // number of slots per bucket
  tpcc::atomic<size_t> bucket_count_;
  CuckooBuckets buckets_;
  tpcc::atomic<size_t> size_;

  static thread_local size_t expected_bucket_count_;
};

template <typename T, typename HashFunction>
thread_local size_t CuckooHashSet<T, HashFunction>::expected_bucket_count_ = 0;

}  // namespace solutions
}  // namespace tpcc
