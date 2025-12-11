#ifndef HFTDERIBIT_SPSC_QUEUE_H
#define HFTDERIBIT_SPSC_QUEUE_H
#include <array>
#include <atomic>
#include <cstddef>
#include <optional>

namespace deribit {
	/**
	 * Single-producer single-consumer ring buffer with a fixed compile time
	 * capacity. The implementation relies on N being a power of two so that
	 * wrap-around can be performed with a fast bitwise and instead of a
	 * modulus operation.
	 *
	 * The queue stores head and tail indices as atomics with explicit
	 * memory ordering to provide a lightweight, lock-free handoff between
	 * one writer and one reader. The storage is aligned to cache lines to
	 * reduce false sharing between producer and consumer.
	 *
	 * Template parameters:
	 * @tparam T Element type stored in the queue.
	 * @tparam N Capacity of the queue; must be a power of two.
	 */
	template<typename T, size_t N>
	class SPSCQueue {
		/** Ensure N is a power of 2 for efficient wrap-around math. */
		static_assert((N & (N - 1)) == 0, "N must be a power of 2");

		/** Aligned ring buffer storage. */
		alignas(64) std::array<T, N> buffer;

		/** Producer index (next slot to write). */
		alignas(64) std::atomic<size_t> head{0};

		/** Consumer index (next slot to read). */
		alignas(64) std::atomic<size_t> tail{0};

	public:
		/**
		 * Attempt to push a value into the queue.
		 *
		 * This is the single-producer fast path. It computes the next head
		 * position using a bitwise wrap, checks for a full queue, stores
		 * the value into the buffer and publishes the new head with
		 * release semantics.
		 *
		 * @param v The value to enqueue.
		 * @return true when the value was enqueued, false if the queue was full.
		 */
		bool push(const T& v) {
			size_t h = head.load(std::memory_order_relaxed);
			// Compute next slot using power-of-two wrap
			size_t next = (h + 1) & (N - 1);

			// Check if the queue is full
			if (next == tail.load(std::memory_order_acquire)) return false;

			// Store the value and publish the new head index
			buffer[h] = v;
			head.store(next, std::memory_order_release);
			return true;
		}

		/**
		 * Attempt to pop a value from the queue.
		 *
		 * This is the single-consumer fast path. It checks whether the
		 * queue is empty, reads the element from the buffer, advances the
		 * tail index with release semantics and returns the popped value.
		 *
		 * @return optional containing the popped element or nullopt when empty.
		 */
		std::optional<T> pop() {
			size_t t = tail.load(std::memory_order_relaxed);
			if (t == head.load(std::memory_order_acquire)) {
				return std::nullopt;
			}

			T v = buffer[t];
			tail.store((t + 1) & (N - 1), std::memory_order_release);
			return v;
		}
	};
}

#endif //HFTDERIBIT_SPSC_QUEUE_H