#ifndef HFTDERIBIT_DERIBIT_CLIENT_H
#define HFTDERIBIT_DERIBIT_CLIENT_H

#include <string>
#include <atomic>

#include "websocket_beast.h"
#include "dispatcher.h"
#include "env.h"
#include "receiver.h"
#include "request_sender.h"
#include "spsc_queue.h"
#include "rate_limiter.h"

namespace deribit {

/**
 * @brief A small client that wires together the websocket, queues,
 * background sender/receiver, and the dispatcher. It provides simple convenience
 * methods to subscribe to channels and send RPCs.
 *
 * The client is responsible for:
 * - Establishing a connection to Deribit (Testnet or Mainnet).
 * - Handling WebSocket communication for subscriptions and RPC requests.
 * - Managing outbound and inbound queues for message dispatch.
 * - Using background threads for request sending and receiving messages.
 */
class DeribitClient : public AccessTokenProvider {

private:
    /* Authentication fields for OAuth2 client credentials flow. */
    std::string client_id; /**< Client ID for authentication */
    std::string client_secret; /**< Client secret for authentication */
    std::string access_token; /**< Access token received after authentication */

    WebSocketBeast ws; /**< WebSocket connection handler */
    Dispatcher dispatcher; /**< Dispatcher to handle incoming messages (RPC & subscription) */

    /** Inbound messages arriving from the websocket (single-consumer). */
    SPSCQueue<std::string, 4096> inbound_queue;

    /** Outbound messages to be sent over the websocket (single-producer). */
    SPSCQueue<std::string, 1024> outbound_queue;

    /** Background receiver that reads from the websocket into inbound_queue. */
    Receiver receiver;

    /** Background sender that pops outbound_queue and writes to the websocket. */
    RequestSender sender;

    /** Connection state flag. */
    std::atomic<bool> connected{false};

    /** Rate limiter for controlling the rate of requests sent. */
    RateLimiter rate_limiter;

    /** Dedicated dispatcher thread. */
    std::thread dispatcher_thread;


public:
    /**
     * @brief Construct the client and wire the receiver and sender to the queues
     * and websocket. The client is initially disconnected; call connect()
     * to establish the underlying network connection and start workers.
     */
    DeribitClient() : receiver(ws, inbound_queue),
                      sender(outbound_queue, ws, this) {
        LOG_DEBUG("Loading credentials from env...");
        load_credentials_from_env();
    }

    /**
     * @brief Callback type used for subscription notifications. The callback is
     * invoked with a ParsedMessage that contains channel and data views.
     */
    using SubCallback = void (*)(const ParsedMessage&);

    /**
     * @brief Load client credentials from environment variables.
     *
     * This helper reads DERIBIT_CLIENT_ID and DERIBIT_CLIENT_SECRET
     * from the environment and stores them in the client instance.
     * It throws if either variable is missing.
     */
    void load_credentials_from_env() {
        client_id     = deribit::get_env("DERIBIT_CLIENT_ID");
        client_secret = deribit::get_env("DERIBIT_CLIENT_SECRET");
    }

    /**
     * @brief Get the access token.
     * @return Reference to the access token string.
     */
    [[nodiscard]] const std::string& get_access_token() const {
        return access_token;
    }

    /**
     * @brief Establish a connection to Deribit (testnet or mainnet depending on
     * the websocket helper configuration) and start the sender and
     * receiver background threads.
     */
    void connect() {
        ws.connect();
        connected = true;

        receiver.start();
        sender.start();

        dispatcher_thread = std::thread(&DeribitClient::dispatch_loop, this);

        authenticate();
    }

    void authenticate() {
        if (client_id.empty() || client_secret.empty()) {
            throw std::runtime_error("Credentials not loaded");
        }

        constexpr uint64_t AUTH_ID = 9001;

        dispatcher.register_rpc(
            AUTH_ID,

            // on_success callback
            [](const ParsedMessage& pm, void* user_ptr) {
                auto* self = static_cast<DeribitClient*>(user_ptr);

                if (pm.access_token.empty()) {
                    LOG_ERROR("Auth success received but no access_token found");
                    return;
                }

                self->access_token = pm.access_token;
                LOG_INFO("Authentication successful. Access token stored.");
            },

            // on_error callback
            [](const ParsedMessage& pm, void*) {
                LOG_ERROR("Authentication failed {} {}", pm.error_code, pm.error_msg);
            },

            this  // user pointer back to instance
        );

        // Build params for client_credentials flow
        std::string params =
            std::string(R"({"grant_type":"client_credentials","client_id":")")
            + client_id +
            R"(","client_secret":")" + client_secret + R"("})";

        send_rpc(AUTH_ID, "public/auth", params);

        LOG_INFO("Auth request sent");
    }


    /**
     * @brief Register a subscription callback for a channel name.
     *
     * The provided callback will be invoked when a notification for the
     * hashed channel is dispatched. The channel string is used as-is and
     * is hashed internally by the dispatcher.
     *
     * @param channel Channel name to register for.
     * @param cb Callback function invoked when notifications arrive.
     */
    void register_subscription(const std::string_view channel, SubCallback cb) {
        dispatcher.register_subscription(channel, cb);
    }

    /**
     * @brief Convenience helper to subscribe to a single channel.
     *
     * This formats a public/subscribe RPC and queues it for sending by the
     * background RequestSender. The request id here is a fixed value for
     * convenience; callers can instead use send_rpc for custom requests.
     *
     * @param channel The subscription channel to subscribe to.
     */
    void subscribe(const std::string& channel) {
        // Check rate limiter before sending request
        if (!rate_limiter.allow_request()) {
            LOG_WARN("Rate limit exceeded, request denied.");
            return;
        }

        std::string msg = std::string(R"({
            "jsonrpc": "2.0",
            "id": 1001,
            "method": "public/subscribe",
            "params": { "channels": [")")
            + channel +
            R"("] }
        })";

        outbound_queue.push(msg);
    }

    /**
     * @brief Send a generic RPC request. The message is formatted and queued for
     * asynchronous transmission by the RequestSender.
     *
     * @param id Numeric request id used to correlate responses.
     * @param method The RPC method name (for example "public/ping").
     * @param params_json Preformatted JSON string for the params field.
     * @return true if the request was queued successfully, false if rate limited.
     */
    bool send_rpc(const uint64_t id, const std::string& method, const std::string& params_json) {
        if (!rate_limiter.allow_request()) {
            LOG_WARN("Rate limit hit for ID {}", id);
            return false;
        }

        const std::string msg = R"({"jsonrpc":"2.0","id":)" + std::to_string(id) +
                          R"(,"method":")" + method + R"(","params":)" + params_json + "}";

        outbound_queue.push(msg);
        return true;
    }

    /**
     * @brief Continuous dispatch loop that runs until the client is closed.
     *
     * This function repeatedly waits for messages on the inbound queue
     * and dispatches them. It can be run in a dedicated thread for
     * continuous processing.
     */
    void dispatch_loop() {
        while (true) {
            auto msg = inbound_queue.wait_and_pop();

            // Shutdown signal
            if (!connected.load(std::memory_order_acquire) || msg.empty()) {
                break;
            }

            simdjson::padded_string padded(msg);
            dispatcher.dispatch(padded);
        }

        LOG_INFO("Dispatcher thread exiting");
    }


    /**
     * @brief Close the client by stopping background workers and closing the
     * underlying websocket connection.
     */
    void close() {
        connected.store(false, std::memory_order_release);

        inbound_queue.push("");      // unblock dispatcher

        receiver.request_stop();     // signal receiver

        // Wait a moment to let receiver exit cleanly
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        sender.stop();               // sender can stop immediately
        receiver.stop();             // now join safely

        if (dispatcher_thread.joinable()) {
            dispatcher_thread.join();
        }
    }


    /**
     * @brief Get a reference to the internal dispatcher.
     * @return Reference to the Dispatcher instance.
     */
    Dispatcher& get_dispatcher() {
        return dispatcher;
    }
};

} // namespace deribit

#endif // HFTDERIBIT_DERIBIT_CLIENT_H
