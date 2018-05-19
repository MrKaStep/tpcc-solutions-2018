#pragma once

#include <tpcc/stdlike/condition_variable.hpp>
#include "rwlock_traits.hpp"

#include <mutex>

namespace tpcc {
namespace solutions {

class ReaderWriterLock {
 public:
  ReaderWriterLock() :
      readers_sections_(0),
      writers_sections_(0),
      writers_waiting_(0),
      writer_active_(false),
      readers_waiting_(0),
      readers_active_(0),
      next_(kNone) {
  }


  // Reader section

  void ReaderLock() {
    std::unique_lock<std::mutex> lock(mutex_);
    ++readers_waiting_;
    readers_allowed_.wait(lock, [this]() {
      return (!this->writer_active_) && this->next_ != kWriter;
    });
    --readers_waiting_;
    ++readers_sections_;
    ++readers_active_;
    writers_sections_ = 0;
    UpdateNext();
  }

  void ReaderUnlock() {
    std::unique_lock<std::mutex> lock(mutex_);
    --readers_active_;
    Notify();
  }

  // Writer section

  void WriterLock() {
    std::unique_lock<std::mutex> lock(mutex_);
    ++writers_waiting_;
    writers_allowed_.wait(lock, [this]() {
      return !this->writer_active_ && this->readers_active_ == 0 && this->next_ != kReader;
    });
    --writers_waiting_;
    ++writers_sections_;
    writer_active_ = true;
    readers_sections_ = 0;
    UpdateNext();
  }

  void WriterUnlock() {
    std::unique_lock<std::mutex> lock(mutex_);
    writer_active_ = false;
    Notify();
  }

 private:
  enum NextUser {
    kNone,
    kWriter,
    kReader
  };

 private:
  static const size_t kReadersSectionsThreshold = 10000;
  static const size_t kWritersSectionsThreshold = 500;
  static const size_t kReadersWritersRatioThreshold = 10;

 private:
  void Notify() {
    if (writers_waiting_ == 0 && readers_waiting_ == 0) {
      return;
    }
    if (writers_waiting_ == 0) {
      readers_allowed_.notify_all();
      return;
    }
    if (readers_waiting_ == 0) {
      if (readers_active_ == 0)
        writers_allowed_.notify_one();
      return;
    }
    if (next_ == kWriter) {
      if (readers_active_ == 0)
        writers_allowed_.notify_one();
    } else if (next_ == kReader) {
      readers_allowed_.notify_all();
    } else {
      if (readers_waiting_ > writers_waiting_ * kReadersWritersRatioThreshold) {
        readers_allowed_.notify_all();
      } else {
        if (readers_active_ == 0)
          writers_allowed_.notify_one();
      }
    }
  }

  void UpdateNext() {
    if (writers_waiting_ > 0 && readers_sections_ > kReadersSectionsThreshold) {
      next_ = kWriter;
    } else if (readers_waiting_ > 0 && writers_sections_ > kWritersSectionsThreshold) {
      next_ = kReader;
    } else {
      next_ = kNone;
    }
  }

 private:
  std::mutex mutex_;
  tpcc::condition_variable readers_allowed_;
  tpcc::condition_variable writers_allowed_;

  size_t readers_sections_;
  size_t writers_sections_;

  size_t writers_waiting_;
  bool writer_active_;

  size_t readers_waiting_;
  size_t readers_active_;

  NextUser next_;
};

template<>
struct RWLockTraits<ReaderWriterLock> {
  static const RWLockPriority kPriority = RWLockPriority::Fair;
};

} // namespace solutions
} // namespace tpcc
