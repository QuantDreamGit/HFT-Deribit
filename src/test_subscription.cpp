#include <iostream>
#include <string>
#include <atomic>

#include "deribit/logging.h"
#include "deribit/websocket_beast.h"
#include "deribit/dispatcher.h"
#include "deribit/request_sender.h"
#include "deribit/spsc_queue.h"
#include "deribit/receiver.h"

/**
 * Integration test that connects to Deribit's Testnet, subscribes to a
 * price index channel and exits once a subscription notification is
 * received. This program exercises the WebSocket, receiver, sender and
 * dispatcher plumbing in a small end-to-end scenario.
 */

/**
 * Global flag used by the test to indicate a subscription notification
 * has been received. The flag is atomic because it is set from the
 * dispatcher callback which may run on a different thread.
 */
std::atomic<bool> subscription_triggered = false;

/**
 * Subscription callback invoked when a subscription notification for
 * the requested channel arrives.
 *
 * @param pm ParsedMessage holding the channel name and the raw data
 *           payload as string views into the original JSON buffer.
 */
void on_price(const deribit::ParsedMessage& pm)
{
    subscription_triggered = true;

    LOG_INFO("Subscription received");
    LOG_INFO("Channel = {}", pm.channel);
    LOG_INFO("Data    = {}", pm.data);
}

/**
 * Main test entry point.
 *
 * The program initializes logging, connects the WebSocket, registers a
 * subscription handler with the dispatcher, starts the sender and
 * receiver background threads, issues a public/subscribe request and
 * then enters a dispatch loop until the subscription callback fires.
 */
int main() {
    // Setup logging
    deribit::init_logging();
    SET_LOG_LEVEL(deribit::LogLevel::DEBUG);

    LOG_INFO("Connecting to Deribit Testnet");

    // Initialize WebSocket, Dispatcher, Sender, Receiver
    deribit::WebSocketBeast ws;
    deribit::Dispatcher dispatcher;

    // Outbound queue for WS sending
    deribit::SPSCQueue<std::string, 1024> outbound_queue;
    deribit::RequestSender sender(outbound_queue, ws);
    // Inbound queue for WS receiving
    deribit::SPSCQueue<std::string, 4096> inbound_queue;
    deribit::Receiver receiver(ws, inbound_queue);

    // Register subscription handler
    dispatcher.register_subscription("deribit_price_index.btc_usd", on_price);

    // Connect WebSocket
    ws.connect();
    // Start sender and receiver threads
    sender.start();
    receiver.start();

    LOG_INFO("Connected. Sending subscription request");

    // Send subscription request
    outbound_queue.push(
        R"({
            "jsonrpc": "2.0",
            "id": 1,
            "method": "public/subscribe",
            "params": { "channels": ["deribit_price_index.btc_usd"] }
        })"
    );

    // Main dispatch loop
    while (!subscription_triggered.load()) {

        auto msg = inbound_queue.pop();
        if (!msg.has_value()) {
            continue;
        }

        LOG_DEBUG("Raw Received: {}", msg.value());
        dispatcher.dispatch(msg->c_str(), msg->size());
    }

    LOG_INFO("Real Deribit subscription test passed");

    receiver.stop();
    sender.stop();
    ws.close();

    return 0;
}
