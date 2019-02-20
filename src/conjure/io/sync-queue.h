#ifndef CONJURE_IO_SYNC_QUEUE_H_
#define CONJURE_IO_SYNC_QUEUE_H_

#include <cmath>
#include <atomic>
#include <mutex>
#include <vector>
#include "conjure/log.h"

namespace conjure::io {

namespace detail {

struct SpinLock {
    using Guard = std::lock_guard<SpinLock>;

    void lock() {
        for (; lock_.test_and_set(std::memory_order_acquire);) continue;
    }
    void unlock() {
        lock_.clear(std::memory_order_release);
    }
    std::atomic_flag lock_;
};

struct PowerOfTwoCeil {

    PowerOfTwoCeil(int n) {
        Init(n);
    }

    void Double() {
        value *= 2;
        ++mask;
    }

    int value;
    uint32_t mask = 0;

  private:
    void Init(int n) {
        int bits = (int)std::ceil(std::log2((double)n));

        value = std::pow(2, bits);

        mask = ~mask;
        mask <<= bits;
        mask = ~mask;
    }
};

template <typename T>
T &operator%=(T &lhs, PowerOfTwoCeil rhs) {
    lhs &= rhs.mask;
    return lhs;
}

} // namespace detail

template <typename T>
class SyncQueue {
    using Lock = detail::SpinLock;

  public:
    static constexpr int kDefaultCapacity = 1024;
    SyncQueue(int capacity = kDefaultCapacity)
        : capacity_(capacity), data_(capacity_.value, T{}) {}

    bool Push(const T &val) {
        Lock::Guard hold(tail_lock_);
        UnsyncPush(val);
    }

    bool UnsyncPush(const T &val) {
        if (Full()) {
            return false;
        }
        int place = tail_++;
        tail_ %= capacity_;
        data_[place] = val;
        ++size_;

        CONJURE_LOGF("size after push: %d", size_.load());

        return true;
    }

    bool Pop(T &store) {
        Lock::Guard hold(head_lock_);
        UnsyncPop(store);
    }

    bool UnsyncPop(T &store) {
        if (Empty()) {
            return false;
        }
        int place = head_++;
        head_ %= capacity_;
        store = data_[place];
        --size_;

        CONJURE_LOGF("size after pop: %d", size_.load());

        return true;
    }

    int Size() const {
        return size_;
    }

    bool Empty() const {
        return size_ == 0;
    }
    bool Full() const {
        return size_ == Capacity();
    }

    int Capacity() const {
        return capacity_.value;
    }

  private:
    detail::PowerOfTwoCeil capacity_;

    std::vector<T> data_;

    volatile int head_ = 0;
    volatile int tail_ = 0;

    std::atomic<int> size_ = 0;

    Lock head_lock_;
    Lock tail_lock_;
};

} // namespace conjure::io

#endif // CONJURE_IO_SYNC_QUEUE_H_
