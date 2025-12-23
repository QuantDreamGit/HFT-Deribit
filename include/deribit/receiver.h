#ifndef HFTDERIBIT_RECEIVER_H
#define HFTDERIBIT_RECEIVER_H

#include <thread>
#include <atomic>

#include "spsc_queue.h"
#include "websocket_beast.h"
#include "logging.h"

namespace deribit {

/**
 * Lightweight background receiver that reads messages from a websocket and
 * forwards them into a single-producer single-consumer queue.
 *
 * The Receiver owns a thread which repeatedly reads from the provided
 * WebSocketBeast instance and tries to push the received text messages
 * into the supplied SPSCQueue. Empty messages are ignored and a full
 * inbound queue causes the message to be dropped with a warning log.
 */
class Receiver {
    /** Thread object that runs the background receive loop. */
    std::thread th;

    /** Atomic flag that controls the running state of the background thread. */
    std::atomic<bool> running{false};

    /** Reference to the websocket instance used for reading incoming messages. */
    WebSocketBeast& ws;

    /** Reference to the inbound queue where messages are enqueued. */
    SPSCQueue<std::string, 4096>& queue;

public:
    /**
     * Construct a Receiver bound to a websocket and a queue.
     *
     * @param ws_ WebSocket instance used for reading messages.
     * @param q Queue instance where incoming messages are pushed.
     */
    Receiver(WebSocketBeast& ws_, SPSCQueue<std::string, 4096>& q)
        : ws(ws_), queue(q) {}

    /**
     * Start the background thread and begin reading messages.
     *
     * This sets the running flag, logs the start event, and launches the
     * receive loop in a separate thread.
     */
    void start() {
        running.store(true, std::memory_order_release);
        LOG_INFO("Receiver thread starting");
        th = std::thread([this]() { run(); });
    }

    /**
     * Stop the background thread and wait for it to terminate.
     *
     * This clears the running flag, joins the thread if it is
     * joinable, and logs the stop event.
     */
    void stop() {
        running.store(false, std::memory_order_release);

        ws.close();

        if (th.joinable())
            th.join();
    }

    /** Request the receiver to stop running. */
    void request_stop() {
        running.store(false, std::memory_order_release);
    }

private:
    /**
     * The main receive loop executed on the background thread.
     *
     * It continuously reads messages from the websocket, ignores empty
     * messages, and attempts to push non-empty messages to the inbound
     * queue. If the queue is full the message is dropped and a warning
     * is emitted.
     */
    void run() const {
        while (running.load(std::memory_order_acquire)) {
            std::string msg = ws.read();
            if (msg.empty()) break;

            queue.push(std::move(msg));
        }

        LOG_DEBUG("Receiver thread exiting");
    }
};

} // namespace deribit

#endif // HFTDERIBIT_RECEIVER_H
