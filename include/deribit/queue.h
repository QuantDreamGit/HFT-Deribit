#ifndef HFT_DERIBIT_QUEUE_H
#define HFT_DERIBIT_QUEUE_H
#include <condition_variable>
#include <mutex>
#include <queue>
#include <utility>

#include "deribit/logging.h"

namespace deribit {

template<typename T>
class ThreadSafeQueue {
	std::queue<T> q;
	std::string name;
	mutable std::mutex mtx;
	std::condition_variable cv;

public:
	explicit ThreadSafeQueue(std::string name) : name(std::move(name)) {};

	void push(const T& val) {
		// Lock the mutex until the end of the scope
		{
			std::lock_guard<std::mutex> lk(mtx);
			q.push(val);
		}
		cv.notify_one();
		LOG_DEBUG("[" + name + "] " + "Pushed value correctly!");
	}

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

	bool empty() {
		std::lock_guard<std::mutex> lk(mtx);
		LOG_DEBUG("[" + name + "] " + "Checked if queue is empty!");
		return q.empty();
	}
};

}

#endif //HFT_DERIBIT_QUEUE_H