#include <iostream>
#include <mutex>
#include <condition_variable>

#include "deribit/logging.h"
#include "deribit/deribit_client.h"

/**
 * Small test program that exercises the DeribitClient subscription path.
 *
 * The program registers a subscription callback for the BTC/USD price
 * index channel, connects the client, issues a subscription request and
 * then blocks until the first subscription notification is received.
 * It is intended as a lightweight integration smoke test.
 */

/**
 * Synchronization primitives used to block the main thread until the
 * first subscription tick is received.
 *
 * The callback is invoked from the client's dispatcher thread, while
 * the main thread waits on the condition variable.
 */
std::mutex tick_mtx;
std::condition_variable tick_cv;
bool tick_received = false;

/**
 * Subscription callback invoked by the client dispatcher when a
 * notification for the subscribed channel arrives.
 *
 * The ParsedMessage provides zero-copy views into the JSON message for
 * channel and data. The callback logs the payload and signals the main
 * thread that the test condition has been satisfied.
 *
 * @param pm Parsed subscription message containing channel and data.
 */
void on_price(const deribit::ParsedMessage& pm)
{
	LOG_INFO("Received subscription tick");
	LOG_INFO("	Channel = {}", pm.channel);
	LOG_INFO("	Data = {}", pm.data);

	{
		std::lock_guard<std::mutex> lock(tick_mtx);
		tick_received = true;
	}
	tick_cv.notify_one();
}

/**
 * Main entry point for the subscription smoke test.
 *
 * Initializes logging, constructs the client, registers the subscription
 * handler, connects to Deribit, subscribes to the BTC/USD price index
 * channel, and then blocks until the first subscription notification
 * is observed.
 *
 * @return Exit code (0 on success).
 */
int main()
{
	deribit::init_logging();
	SET_LOG_LEVEL(deribit::LogLevel::DEBUG);

	deribit::DeribitClient client;

	client.register_subscription(
		"deribit_price_index.btc_usd", on_price
	);

	LOG_INFO("Connecting...");
	client.connect();

	LOG_INFO("Subscribing...");
	client.subscribe("deribit_price_index.btc_usd");

	/* Block until the subscription callback signals completion */
	{
		std::unique_lock<std::mutex> lock(tick_mtx);
		tick_cv.wait(lock, [] { return tick_received; });
	}

	LOG_INFO("Subscription test passed");
	client.close();

	return 0;
}
