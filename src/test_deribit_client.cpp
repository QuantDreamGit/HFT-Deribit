#include <iostream>
#include <atomic>

#include "deribit/logging.h"
#include "deribit/deribit_client.h"

/**
 * Small test program that exercises the DeribitClient subscription path.
 *
 * The program registers a subscription callback for the BTC/USD price
 * index channel, connects the client, issues a subscription request and
 * then polls the client until the first subscription notification is
 * received. It is intended as a lightweight integrational smoke test.
 */

/**
 * Atomic flag set by the subscription callback when a tick arrives.
 *
 * The flag is atomic because it may be set from the dispatcher's
 * callback context which can run on a different thread than main.
 */
std::atomic<bool> got_tick = false;

/**
 * Subscription callback invoked by the client dispatcher when a
 * notification for the subscribed channel arrives.
 *
 * The ParsedMessage provides zero-copy views into the JSON message for
 * channel and data; the callback simply logs the payload and sets the
 * test completion flag.
 */
void on_price(const deribit::ParsedMessage& pm)
{
	got_tick = true;
	LOG_INFO("Received subscription tick");
	LOG_INFO("Channel = {}", pm.channel);
	LOG_INFO("Data = {}", pm.data);
}

/**
 * Main entry point for the subscription smoke test.
 *
 * Initializes logging, constructs the client, registers the subscription
 * handler, connects, subscribes and then repeatedly polls the client
 * until a subscription notification is observed.
 */
int main() {
	deribit::init_logging();
	SET_LOG_LEVEL(deribit::LogLevel::DEBUG);

	deribit::DeribitClient client;

	client.register_subscription("deribit_price_index.btc_usd", on_price);

	LOG_INFO("Connecting...");
	client.connect();

	LOG_INFO("Subscribing...");
	client.subscribe("deribit_price_index.btc_usd");

	// Main processing loop
	while (!got_tick.load()) {
		client.poll();
	}

	LOG_INFO("Subscription test passed");
	client.close();
	return 0;
}
