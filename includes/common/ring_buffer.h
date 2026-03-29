#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <vector>
#include <atomic>
#include <cstddef>
#include "../../src/common/protocol.h"

template <typename T, size_t Size>
class RingBuffer {
public:
    static_assert((Size & (Size - 1)) == 0, "Size must be a power of 2 for fast masking");

    RingBuffer() : head_(0), tail_(0) {
        buffer_.resize(Size);
    }

    // Producer calls this
    bool push(const T& item) {
        size_t head = head_.load(std::memory_order_relaxed);
        size_t next_head = (head + 1) & (Size - 1);

        if (next_head == tail_.load(std::memory_order_acquire)) {
            return false; // Buffer full
        }

        buffer_[head] = item;
        head_.store(next_head, std::memory_order_release);
        return true;
    }

    // Consumer calls this
    bool pop(T& item) {
        size_t tail = tail_.load(std::memory_order_relaxed);

        if (tail == head_.load(std::memory_order_acquire)) {
            return false; // Buffer empty
        }

        item = buffer_[tail];
        tail_.store((tail + 1) & (Size - 1), std::memory_order_release);
        return true;
    }

private:
    std::vector<T> buffer_;
    
    // Align to cache lines to prevent "False Sharing"
    alignas(64) std::atomic<size_t> head_;
    alignas(64) std::atomic<size_t> tail_;
};

#endif