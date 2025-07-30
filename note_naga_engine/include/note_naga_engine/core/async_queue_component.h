#pragma once

#include <note_naga_engine/core/lock_free_mpmc_queue.h>
#include <note_naga_engine/logger.h>
#include <note_naga_engine/note_naga_api.h>

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

// empty notify message type
typedef NOTE_NAGA_ENGINE_API struct NN_AsyncTriggerMessage_t {
    // This is an empty message type used to trigger the queue processing
    // It can be extended with additional fields if needed in the future
    // Currently, it serves as a simple notification mechanism
    NN_AsyncTriggerMessage_t() = default;
} NN_AsyncTriggerMessage_t;

/**
 * @brief Abstract class for Note Naga components utilizing a lock-free MPMC queue.
 *        Supports safe thread lifetime management.
 * @tparam T Type of data in the queue.
 * @tparam QueueSize Capacity of the queue (must be a power of 2).
 *
 * @example How to use:
 *
 * struct MyAudioData {
 *    int sample;
 * };
 *
 * class MyAudioComponent : public AsyncQueueComponent<MyAudioData, 1024> {
 * public:
 *     void onItem(const MyAudioData& data) override {
 *         std::cout << "Processing sample: " << data.sample << std::endl;
 *     }
 * };
 * int main() {
 *     MyAudioComponent component;
 *
 *     for (int i = 0; i < 10; ++i) {
 *         MyAudioData data{i};
 *         component.pushToQueue(data);
 *     }
 *     component.killThread();
 *     return 0;
 * }
 */
template <typename T, size_t QueueSize>
class NOTE_NAGA_ENGINE_API AsyncQueueComponent {
public:
    AsyncQueueComponent()
        : m_queue(std::make_unique<LockFreeMPMCQueue<T, QueueSize>>()),
          m_stopThread(false) {
        m_thread = std::thread([this]() { this->threadFunc(); });
        NOTE_NAGA_LOG_INFO("Engine Component initialized with queue size: " +
                           std::to_string(QueueSize));
    }

    // Not copyable/movable
    AsyncQueueComponent(const AsyncQueueComponent &) = delete;
    AsyncQueueComponent &operator=(const AsyncQueueComponent &) = delete;

    virtual ~AsyncQueueComponent() { killThread(); }

    /**
     * @brief Enqueues data into the queue (thread-safe, supports multiple producers).
     */
    bool pushToQueue(const T &value) {
        bool ok = m_queue->enqueue(value);
        if (ok) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_cv.notify_one();
        }
        return ok;
    }

    /**
     * @brief Cleanly terminates the worker thread (blocking).
     */
    void killThread() {
        m_stopThread.store(true, std::memory_order_release);
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_cv.notify_one();
        }
        if (m_thread.joinable()) m_thread.join();
        NOTE_NAGA_LOG_INFO("Engine Component thread killed");
    }

protected:
    /**
     * @brief Called when a value is dequeued. Override in your derived class.
     */
    virtual void onItem(const T &value) = 0;

private:
    void threadFunc() {
        while (!m_stopThread.load(std::memory_order_acquire)) {
            std::optional<T> item = m_queue->dequeue();
            if (item) {
                onItem(*item);
                continue; // try next immediately (avoid sleeping)
            }
            // Sleep until notified of new data or kill
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this]() {
                return m_stopThread.load(std::memory_order_acquire) || !m_queue->empty();
            });
        }
    }

    std::unique_ptr<LockFreeMPMCQueue<T, QueueSize>> m_queue;

    std::thread m_thread;
    std::atomic<bool> m_stopThread;
    std::mutex m_mutex;
    std::condition_variable m_cv;
};