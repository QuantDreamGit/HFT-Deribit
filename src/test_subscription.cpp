#include <iostream>
#include <mutex>
#include <condition_variable>

#include "deribit/logging.h"
#include "deribit/deribit_client.h"

/**
 * Integration test that connects to Deribit's Testnet, subscribes to a
 * price index channel and exits once a subscription notification is
 * received. This program exercises the DeribitClient subscription
 * plumbing in a small end-to-end scenario.
 */

/**
 * Synchronization primitives used to block the main thread until the
 * first subscription tick is received.
 *
 * The callback is invoked from the client's dispatcher thread, while
 * the main thread waits on the condition variable.
 */
std::mutex sub_mtx;
std::condition_variable sub_cv;
bool subscription_triggered = false;

/**
 * Subscription callback invoked when a subscription notification for
 * the requested channel arrives.
 *
 * @param pm ParsedMessage holding the channel name and the raw data
 *           payload as string views into the original JSON buffer.
 */
void on_price(const deribit::ParsedMessage& pm)
{
	LOG_INFO("Subscription received");
	LOG_INFO("Channel = {}", pm.channel);
	LOG_INFO("Data    = {}", pm.data);

	{
		std::lock_guard<std::mutex> lock(sub_mtx);
		subscription_triggered = true;
	}
	sub_cv.notify_one();
}

/**
 * Main test entry point.
 *
 * The program initializes logging, constructs the DeribitClient,
 * registers a subscription handler, connects to Deribit, issues a
 * public/subscribe request and blocks until the first notification
 * is received.
 *
 * @return Exit code (0 on success).
 */
int main()
{
	// Setup logging
	deribit::init_logging();
	SET_LOG_LEVEL(deribit::LogLevel::DEBUG);

	LOG_INFO("Connecting to Deribit Testnet");

	deribit::DeribitClient client;

	// Register subscription handler
	client.register_subscription(
		"deribit_price_index.btc_usd",
		on_price
	);

	// Connect client (starts WS, sender, receiver, dispatcher)
	client.connect();

	LOG_INFO("Connected. Sending subscription request");

	// Issue subscription request
	client.subscribe("deribit_price_index.btc_usd");

	// Block until subscription callback fires
	{
		std::unique_lock<std::mutex> lock(sub_mtx);
		sub_cv.wait(lock, [] { return subscription_triggered; });
	}

	LOG_INFO("Real Deribit subscription test passed");

	client.close();

	return 0;
}
