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

namespace tpcc {
namespace solutions {

////////////////////////////////////////////////////////////////////////////////

class TTASSpinLock {
 public:
  void lock() {
    // to be implemented
  }

  void unlock() {
    // to be implemented
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
    return {};  // to be implemented
  }

  LockSet Lock(size_t i, size_t j) {
    return {};  // to be implemented
  }

  LockSet Lock(std::vector<size_t> indices) {
    return {};  // to be implemented
  }

  LockSet LockAllFrom(size_t start) {
    return {};  // to be implemented
  }

 private:
  std::vector<Mutex> locks_;
};

////////////////////////////////////////////////////////////////////////////////

struct ElementHash {
  size_t hash_value_;
  uint16_t alt_value_;

  bool operator==(const ElementHash& that) const {
    return hash_value_ == that.hash_value_ && alt_value_ == that.alt_value_;
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
    return ((hash_value >> 48) ^ (hash_value >> 32) ^ (hash_value >> 16) ^
            hash_value) &
           0xFFFF;
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

 public:
  explicit CuckooHashSet(const size_t concurrency_level = 32,
                         const size_t bucket_width = 4)
      : lock_manager_(concurrency_level),
        bucket_width_(bucket_width),
        bucket_count_(GetInitialBucketCount(concurrency_level)),
        buckets_(CreateEmptyTable(bucket_count_, bucket_width_)) {
  }

  bool Insert(const T& element) {
    auto hash = hasher_(element);

    while (true) {
      RememberBucketCount();

      try {
        TwoBuckets buckets;
        LockSet bucket_locks;
        LockTwoBuckets(hash, buckets, bucket_locks);

        // to be continued

      } catch (const TableExpanded& _) {
        LOG_SIMPLE("Insert interrupted due to concurrent table expansion");
      } catch (const TableOvercrowded& _) {
        LOG_SIMPLE("Cannot insert element, table overcrowded");
        ExpandTable();
      }
    }

    UNREACHABLE();
  }

  bool Remove(const T& element) {
    UNUSED(element);
    return false;  // to be implemented
  }

  bool Contains(const T& element) const {
    UNUSED(element);
    return false;  // to be implemented
  }

  size_t GetSize() const {
    return 0;  // to be implemented
  }

 private:
  static CuckooBuckets CreateEmptyTable(const size_t bucket_count,
                                        const size_t bucket_width) {
    return CuckooBuckets(bucket_count, CuckooBucket(bucket_width));
  }

  static size_t GetInitialBucketCount(const size_t concurrency_level) {
    return concurrency_level;  // or something better
  }

  static size_t HashToBucket(const size_t hash_value,
                             const size_t bucket_count) {
    return 0;  // to be implemented
  }

  size_t HashToBucket(const size_t hash_value) const {
    return HashToBucket(hash_value, expected_bucket_count_);
  }

  size_t GetPrimaryBucket(const ElementHash& hash) const {
    return HashToBucket(hash.hash_value_);
  }

  size_t GetAlternativeBucket(const ElementHash& hash) const {
    return HashToBucket(hash.hash_value_ ^ hash.alt_value_);
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
    return 0;  // to be implemented
  }

  LockSet LockTwoBuckets(const TwoBuckets& buckets) const {
    return {};  // to be implemented
  }

  void LockTwoBuckets(const ElementHash& hash, TwoBuckets& buckets,
                      LockSet& locks) const {
    buckets = GetBuckets(hash);
    locks = LockTwoBuckets(buckets);
  }

  // returns false if conflict detected during optimistic move
  bool TryMoveHoleBackward(const CuckooPath& path) {
    UNUSED(path);
    return false;  // to be implemented
  }

  bool TryFindCuckooPathWithRandomWalk(size_t start_bucket, CuckooPath& path) {
    UNUSED(start_bucket);
    UNUSED(path);
    return false;  // to be implemented
  }

  CuckooPath FindCuckooPath(const TwoBuckets& start_buckets) {
    UNUSED(start_buckets);
    for (size_t i = 0; i < kFindPathIterations; ++i) {
      // to be implemented
    }

    throw TableOvercrowded{};
  }

  void EvictElement(const TwoBuckets& buckets) {
    for (size_t i = 0; i < kEvictIterations; ++i) {
      // to be implemented
    }

    throw TableOvercrowded{};
  }

  void ExpandTable() {
    // to be implemented
  }

 private:
  CuckooHasher<T, HashFunction> hasher_;

  mutable LockManager lock_manager_;

  size_t bucket_width_;  // number of slots per bucket
  tpcc::atomic<size_t> bucket_count_;
  CuckooBuckets buckets_;

  static thread_local size_t expected_bucket_count_;
};

template <typename T, typename HashFunction>
thread_local size_t CuckooHashSet<T, HashFunction>::expected_bucket_count_ = 0;

}  // namespace solutions
}  // namespace tpcc
