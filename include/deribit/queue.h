#ifndef HFT_DERIBIT_QUEUE_H
#define HFT_DERIBIT_QUEUE_H
#include <condition_variable>
#include <mutex>
#include <queue>
#include <utility>

#include "deribit/logging.h"

namespace deribit {

/**
 * Thread-safe queue wrapper providing simple push, pop and empty operations.
 *
 * The queue serializes access using a mutex and notifies waiting threads
 * via a condition variable when new items are pushed. The instance name
 * is used to annotate log messages so the source of events can be
 * identified in multi-queue systems.
 *
 * @tparam T Type of elements stored in the queue.
 */
template<typename T>
class ThreadSafeQueue {
    std::queue<T> q;
    std::string name;
    mutable std::mutex mtx;
    std::condition_variable cv;

public:
    /**
     * Construct a named queue instance for logging and identification.
     *
     * @param name Human readable name associated with this queue.
     */
    explicit ThreadSafeQueue(std::string name) : name(std::move(name)) {};

    /**
     * Push a value into the queue and wake a single waiting consumer.
     *
     * The function locks the internal mutex for the duration of the
     * push to ensure the queue state remains consistent, then it
     * signals the condition variable so one waiting thread can proceed.
     *
     * @param val Value to enqueue.
     */
    void push(const T& val) {
        {
            std::lock_guard<std::mutex> lk(mtx);
            q.push(val);
        }
        cv.notify_one();
        LOG_DEBUG("[" + name + "] " + "Pushed value correctly!");
    }

    /**
     * Remove and return the front element if present.
     *
     * If the queue is non-empty this returns the front element and
     * removes it. If the queue is empty a default-constructed T is
     * returned and a warning is logged. This function holds the mutex
     * only for the duration required to check and modify the queue.
     *
     * @return The popped element or a default-constructed T when empty.
     */
    T pop() {
        {
            std::lock_guard<std::mutex> lk(mtx);

            if (!q.empty()) {
                T val = q.front();
                q.pop();
                LOG_DEBUG("[" + name + "] " + "Popped value correctly!");
                return val;
            } else {
                LOG_WARN("[" + name + "] " + "No data in queue to pop");
                return T();
            }
        }
    }

    /**
     * Check whether the queue currently contains no elements.
     *
     * The check is synchronized but represents only a snapshot of the
     * queue state; the result may immediately become stale in concurrent
     * contexts, so this should be used for informational purposes only.
     *
     * @return true if the queue is empty, false otherwise.
     */
    bool empty() {
        std::lock_guard<std::mutex> lk(mtx);
        LOG_DEBUG("[" + name + "] " + "Checked if queue is empty!");
        return q.empty();
    }
};

} // namespace deribit

#endif //HFT_DERIBIT_QUEUE_H