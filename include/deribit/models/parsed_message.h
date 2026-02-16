#ifndef HFTDERIBIT_PARSED_MESSAGE_H
#define HFTDERIBIT_PARSED_MESSAGE_H
#include <cstdint>
#include <string_view>

namespace deribit {

/**
 * Lightweight container holding metadata extracted from a JSON message.
 *
 * The fields are intentionally simple and store views into the original
 * JSON buffer where possible to avoid unnecessary allocations. Use the
 * boolean flags to determine which of the remaining fields are valid.
 */
struct ParsedMessage {
    /** Access token string when present. */
    std::string access_token;

    /** Whether this message represents a response to a previously sent RPC. */
    bool is_rpc = false;

    /** Whether this message is a subscription notification pushed by the server. */
    bool is_subscription = false;

    /** Whether the message contains an error payload. */
    bool is_error = false;

    /** The numeric identifier that correlates RPC responses with requests. */
    uint64_t id = 0;

    /** Numeric error code when is_error is true. */
    uint64_t error_code = 0;

    /** Timestamp in microseconds indicating when the message was received. */
    uint64_t usIn = 0;

    /** Timestamp in microseconds indicating when the message was sent. */
    uint64_t usOut = 0;

    /** Round-trip timing difference in microseconds when available. */
    uint64_t usDiff = 0;

    /** Subscription channel name for notifications (zero-copy view). */
    std::string_view channel;

    /** Raw JSON payload for subscription notifications (zero-copy view). */
    std::string_view data;

    /** Raw JSON result for RPC responses (zero-copy view). */
    std::string_view result;

    /** Error message text when the message carries an error (zero-copy view). */
    std::string_view error_msg;
};

} // namespace deribit

#endif //HFTDERIBIT_PARSED_MESSAGE_H