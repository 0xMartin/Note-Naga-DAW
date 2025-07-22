#pragma once

#include <note_naga_engine/note_naga_api.h>

#include <atomic>
#include <cassert>
#include <cstddef>
#include <memory>
#include <optional>

/**
 * @brief Lock-free Multi-Producer Multi-Consumer queue (bounded).
 *        Fixed-size, does NOT overwrite old data if full.
 *        Thread-safe for multiple producers and consumers.
 *        Based on Dmitry Vyukov's algorithm:
 * http://www.1024cores.net/home/lock-free-algorithms/queues/bounded-mpmc-queue
 * @tparam T Element type.
 * @tparam N Capacity (must be power of 2).
 */
template <typename T, size_t N> class NOTE_NAGA_ENGINE_API LockFreeMPMCQueue {
    static_assert((N & (N - 1)) == 0, "Capacity must be power of 2");

    struct Cell {
        std::atomic<size_t> seq;
        T data;
    };

    Cell buffer[N];
    std::atomic<size_t> head{0};
    std::atomic<size_t> tail{0};

public:
    LockFreeMPMCQueue() {
        for (size_t i = 0; i < N; ++i) {
            buffer[i].seq.store(i, std::memory_order_relaxed);
        }
    }

    LockFreeMPMCQueue(const LockFreeMPMCQueue &) = delete;
    LockFreeMPMCQueue &operator=(const LockFreeMPMCQueue &) = delete;

    bool enqueue(const T &value) {
        Cell *cell;
        size_t pos = head.load(std::memory_order_relaxed);

        for (;;) {
            cell = &buffer[pos & (N - 1)];
            size_t seq = cell->seq.load(std::memory_order_acquire);
            intptr_t dif = (intptr_t)seq - (intptr_t)pos;
            if (dif == 0) {
                if (head.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                    break;
                }
            } else if (dif < 0) {
                // Full
                return false;
            } else {
                pos = head.load(std::memory_order_relaxed);
            }
        }

        cell->data = value;
        cell->seq.store(pos + 1, std::memory_order_release);
        return true;
    }

    std::optional<T> dequeue() {
        Cell *cell;
        size_t pos = tail.load(std::memory_order_relaxed);

        for (;;) {
            cell = &buffer[pos & (N - 1)];
            size_t seq = cell->seq.load(std::memory_order_acquire);
            intptr_t dif = (intptr_t)seq - (intptr_t)(pos + 1);
            if (dif == 0) {
                if (tail.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                    break;
                }
            } else if (dif < 0) {
                // Empty
                return std::nullopt;
            } else {
                pos = tail.load(std::memory_order_relaxed);
            }
        }

        T value = cell->data;
        cell->seq.store(pos + N, std::memory_order_release);
        return value;
    }

    bool empty() const {
        // Not strictly lock-free but fast enough for most checks
        return size() == 0;
    }

    size_t size() const {
        size_t h = head.load(std::memory_order_acquire);
        size_t t = tail.load(std::memory_order_acquire);
        return h - t;
    }
};