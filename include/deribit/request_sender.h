#ifndef HFTDERIBIT_REQUEST_SENDER_H
#define HFTDERIBIT_REQUEST_SENDER_H

#include <thread>
#include <atomic>
#include <string>

#include "spsc_queue.h"
#include "rate_limiter.h"
#include "websocket_beast.h"
#include "logging.h"

namespace deribit {

    /**
     * Minimal interface for objects that can provide an access token.
     * This avoids circular dependencies on DeribitClient.
     */
    struct AccessTokenProvider {
        [[nodiscard]] virtual const std::string& get_access_token() const = 0;
        virtual ~AccessTokenProvider() = default;
    };

/**
 * Background worker that takes outbound requests from a queue and sends
 * them over a websocket, subject to rate limiting.
 *
 * The RequestSender owns a thread which continuously pops strings from
 * the provided SPSCQueue. Before sending each message it consults a
 * token-bucket RateLimiter to enforce an upper bound on request rate.
 */
class RequestSender {
    /** Reference to the outbound queue that supplies JSON request strings. */
    SPSCQueue<std::string, 1024>& queue;

    /** Reference to the active websocket used to transmit requests. */
    WebSocketBeast& ws;

    /** Rate limiter instance used to throttle outgoing requests. */
    RateLimiter rl;

    /** Worker thread that executes the send loop. */
    std::thread worker;

    /** Atomic flag that controls whether the worker loop should keep running. */
    std::atomic<bool> running {false};

    /** Pointer back to the owning DeribitClient (if needed). */
    AccessTokenProvider* auth;

public:
    /**
     * Construct a RequestSender bound to an outbound queue and websocket.
     *
     * @param q Reference to the queue that provides outbound messages.
     * @param ws_ref Reference to the websocket used for sending.
     * @param auth_ptr Pointer to an AccessTokenProvider for private RPCs.
     */
    RequestSender(SPSCQueue<std::string, 1024>& q,
                  WebSocketBeast& ws_ref,
                  AccessTokenProvider* auth_ptr)
        : queue(q), ws(ws_ref), auth(auth_ptr) {}

    /**
     * Start the sender thread.
     *
     * This function launches a background thread which repeatedly:
     * - waits for the rate limiter to allow a request,
     * - pops a value from the outbound queue if available,
     * - sends the popped string over the websocket.
     *
     * The loop continues until `stop()` is called.
     */
    void start() {
        running = true;

        worker = std::thread([this] {

            LOG_INFO("Sender thread started");

            while (running.load()) {

                while (!rl.allow_request()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }

                auto req = queue.pop();
                if (!req) continue;

                std::string msg = std::move(*req);

                // Only uses the interface, not DeribitClient
                if (msg.find("\"private/") != std::string::npos) {

                    const std::string& token = auth->get_access_token();

                    if (!token.empty()) {
                        size_t pos = msg.rfind('}');
                        if (pos != std::string::npos) {
                            msg.insert(pos,
                                R"(,"access_token":")" + token + "\"");
                        }
                    } else {
                        LOG_WARN("Attempted private RPC but access token is empty");
                    }
                }

                ws.send(msg);
            }
        });
    }

    /**
     * Stop the sender thread and wait for it to finish.
     *
     * This clears the running flag and joins the worker thread if it was
     * started, ensuring a clean shutdown.
     */
    void stop() {
        running = false;
        if (worker.joinable()) worker.join();
    }
};

} // namespace deribit


#endif //HFTDERIBIT_REQUEST_SENDER_H