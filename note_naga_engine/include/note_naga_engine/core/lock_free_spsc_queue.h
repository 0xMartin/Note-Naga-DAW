#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <optional>

/**
 * @brief A simple lock-free single-producer single-consumer queue.
 *        Fixed-size, overwrites old data if full.
 *        Not thread-safe for multiple producers or consumers.
 * @tparam T Element type.
 * @tparam N Capacity.
 */
template <typename T, size_t N> class LockFreeSPSCQueue {
    static_assert((N & (N - 1)) == 0, "Capacity must be power of 2 for mask");

    T buffer[N];
    std::atomic<size_t> head{0}; // write index (producer)
    std::atomic<size_t> tail{0}; // read index (consumer)
    static constexpr size_t MASK = N - 1;

public:
    LockFreeSPSCQueue() = default;

    // Not copyable/movable
    LockFreeSPSCQueue(const LockFreeSPSCQueue &) = delete;
    LockFreeSPSCQueue &operator=(const LockFreeSPSCQueue &) = delete;

    bool enqueue(const T &value) {
        size_t h = head.load(std::memory_order_relaxed);
        size_t t = tail.load(std::memory_order_acquire);

        if (((h + 1) & MASK) == (t & MASK)) {
            // Full, overwrite oldest
            tail.store((t + 1) & MASK, std::memory_order_release);
        }
        buffer[h & MASK] = value;
        head.store((h + 1) & MASK, std::memory_order_release);
        return true;
    }

    std::optional<T> dequeue() {
        size_t t = tail.load(std::memory_order_relaxed);
        size_t h = head.load(std::memory_order_acquire);

        if ((t & MASK) == (h & MASK)) {
            // Empty
            return std::nullopt;
        }
        T val = buffer[t & MASK];
        tail.store((t + 1) & MASK, std::memory_order_release);
        return val;
    }

    bool empty() const {
        return (tail.load(std::memory_order_acquire) & MASK) ==
               (head.load(std::memory_order_acquire) & MASK);
    }
};