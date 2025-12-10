#include "deribit/logging.h"
#include "deribit/queue.h"

int main () {
	deribit::init_logging();\
	auto q = deribit::ThreadSafeQueue<int> ("test");
	q.push(42);
	auto val = q.pop();
	LOG_INFO(val);
	val = q.pop();


	LOG_INFO("Test!");
}