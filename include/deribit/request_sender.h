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

public:
    /**
     * Construct a RequestSender bound to an outbound queue and websocket.
     *
     * @param q Reference to the queue that provides outbound messages.
     * @param ws_ref Reference to the websocket used for sending.
     */
    RequestSender(SPSCQueue<std::string, 1024>& q,
                  WebSocketBeast& ws_ref)
        : queue(q), ws(ws_ref) {}

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

                // Rate limiter gate
                while (!rl.allow_request()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }

                // Pop outbound request
                auto req = queue.pop();
                if (!req.has_value()) {
                    continue; // nothing to send yet
                }

                // Send to websocket
                ws.send(req.value());
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