#ifndef HFTDERIBIT_DERIBIT_CLIENT_H
#define HFTDERIBIT_DERIBIT_CLIENT_H

#include <string>
#include <atomic>
#include <future>

#include "account_manager.h"
#include "../network/websocket_beast.h"
#include "dispatcher.h"
#include "../config/env.h"
#include "../network/receiver.h"
#include "../network/request_sender.h"
#include "../infra/spsc_queue.h"
#include "../infra/rate_limiter.h"

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

    /** Atomic counter for generating unique RPC request IDs. */
    std::atomic<uint64_t> rpc_id_counter{1};

public:
    /** Account manager for handling account-related RPCs and state. */
    deribit::AccountManager account_manager;

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

        // Initialize account manager
        account_manager.initialize(*this);
    }

    void connect_sync() {
        ws.connect();
        connected = true;

        receiver.start();
        sender.start();

        dispatcher_thread = std::thread(&DeribitClient::dispatch_loop, this);

        authenticate_sync();

        // Initialize account manager
        account_manager.initialize(*this);
    }

    void authenticate_sync() {
        if (client_id.empty() || client_secret.empty()) {
            LOG_ERROR("Credentials not loaded");
            return;
        }

        std::string params =
            std::string(R"({"grant_type":"client_credentials","client_id":")")
            + client_id +
            R"(","client_secret":")" + client_secret + R"("})";

        ParsedMessage pm = send_rpc_sync("public/auth", params);

        if (pm.is_error) {
            LOG_ERROR("Authentication failed: ", pm.error_msg);
            return;
        }

        if (pm.access_token.empty()) {
            LOG_ERROR("Authentication succeeded but no access_token received");
            return;
        }

        access_token = pm.access_token;

        LOG_INFO("Authentication successful (sync).");
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
            "id": )")
            + std::to_string(next_rpc_id()) +
            R"(,
            "method": "public/subscribe",
            "params": { "channels": [")"
            + channel +
            R"("] }
            })";

        outbound_queue.push(msg);
    }

    /**
     * @brief Convenience helper to register a subscription handler and subscribe to a channel.
     *
     * This overload allows you to provide a callback handler for the subscription
     * notifications at the same time as sending the subscribe request. The handler
     * will be registered before the request is sent to ensure that notifications
     * are not missed.
     *
     * @param channel The subscription channel to subscribe to.
     * @param handler Callable invoked when notifications for the channel are received.
     */
    template<typename Handler>
    void subscribe(const std::string& channel, Handler&& handler)
    {
        dispatcher.register_subscription(channel, std::forward<Handler>(handler));
        subscribe(channel); // send RPC
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

    bool set_rpc_dispatch_handler(const uint64_t id,
                              void (*on_success)(const ParsedMessage&, void*),
                              void (*on_error)(const ParsedMessage&, void*),
                              void* user_data) {
        // Register the RPC handler for the given ID
        // It will automatically be cleared after invocation to prevent stale
        // handlers for future requests with the same ID
        dispatcher.register_rpc(id, on_success, on_error, user_data);

        return true;
    }

    /**
     * @brief Send an RPC request and wait synchronously for the response.
     *
     * This helper sends an RPC request and blocks until a response is received
     * or a timeout occurs. It uses a promise/future pair to wait for the
     * response, and registers temporary RPC handlers that will set the promise
     * value when the response arrives. The handlers are automatically cleared
     * after invocation to prevent stale handlers for future requests with the
     * same ID.
     *
     * @param method The RPC method name (e.g. "public/ping").
     * @param params_json Preformatted JSON string for the params field.
     * @param timeout Duration to wait for a response before throwing a timeout error.
     * @return ParsedMessage containing either the result or error information.
     * @throws std::runtime_error if the request is rate limited or if a timeout occurs.
     */
    ParsedMessage send_rpc_sync(
        const std::string& method,
        const std::string& params_json,
        std::chrono::milliseconds timeout = std::chrono::seconds(5),
        int max_retries = 5)
    {
        int attempt = 0;
        std::chrono::milliseconds backoff{200};

        while (attempt < max_retries) {
            ++attempt;

            const uint64_t id = next_rpc_id();

            std::promise<ParsedMessage> promise;
            auto future = promise.get_future();

            dispatcher.register_rpc(
                id,
                [](const ParsedMessage& pm, void* user_ptr) {
                    auto* p = static_cast<std::promise<ParsedMessage>*>(user_ptr);
                    p->set_value(pm);
                },
                [](const ParsedMessage& pm, void* user_ptr) {
                    auto* p = static_cast<std::promise<ParsedMessage>*>(user_ptr);
                    p->set_value(pm);
                },
                &promise
            );

            // Try sending
            if (!send_rpc(id, method, params_json)) {
                LOG_WARN("Rate limited (attempt {}/{})", attempt, max_retries);
                std::this_thread::sleep_for(backoff);
                backoff *= 2;  // exponential backoff
                continue;
            }

            // Wait for response
            if (future.wait_for(timeout) == std::future_status::ready) {
                return future.get();
            }

            LOG_WARN("RPC timeout (attempt {}/{})", attempt, max_retries);

            // Optional: small sleep before retry
            std::this_thread::sleep_for(backoff);
            backoff *= 2;
        }

        throw std::runtime_error("RPC failed after max retries");
    }


    /**
     * @brief Send an RPC request and return a future for the response.
     *
     * This helper sends an RPC request and returns a std::future that will be
     * set when the response arrives. It uses a shared_ptr to a promise to allow
     * the RPC handlers to set the value when the response is received. The
     * handlers are automatically cleared after invocation to prevent stale
     * handlers for future requests with the same ID.
     *
     * @param method The RPC method name (e.g. "public/ping").
     * @param params_json Preformatted JSON string for the params field.
     * @return std::future<ParsedMessage> that will hold the response when it arrives.
     */
    std::future<ParsedMessage> send_rpc_async(
    const std::string& method,
    const std::string& params_json)
    {
        const uint64_t id = next_rpc_id();
        // Use a shared_ptr to allow the RPC handlers to set the promise value
        auto promise = std::make_shared<std::promise<ParsedMessage>>();
        auto future  = promise->get_future();
        // Register RPC handlers that will set the promise value on success or error
        dispatcher.register_rpc(
            id,
            [](const ParsedMessage& pm, void* user_ptr) {
                auto* p = static_cast<std::promise<ParsedMessage>*>(user_ptr);
                p->set_value(pm);
            },
            [](const ParsedMessage& pm, void* user_ptr) {
                auto* p = static_cast<std::promise<ParsedMessage>*>(user_ptr);
                p->set_value(pm);
            },
            promise.get()
        );

        // Send the RPC request
        send_rpc(id, method, params_json);
        // Return the future to the caller so they can wait for the response asynchronously
        return future;
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

    /**
     * @brief Generate the next unique RPC request ID.
     *
     * This function atomically increments the internal counter and returns
     * the new value. It uses relaxed memory ordering since the ID generation
     * does not need to synchronize with other operations.
     *
     * @return A unique uint64_t ID for RPC requests.
     */
    uint64_t next_rpc_id() noexcept {
        return rpc_id_counter.fetch_add(1, std::memory_order_relaxed);
    }
};

} // namespace deribit

#endif // HFTDERIBIT_DERIBIT_CLIENT_H
