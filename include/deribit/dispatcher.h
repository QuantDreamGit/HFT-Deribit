#ifndef HFTDERIBIT_DISPATCHER_H
#define HFTDERIBIT_DISPATCHER_H

#include <simdjson.h>

#include "fast_hash.h"
#include "rpc_handler.h"

namespace deribit {

/**
 * Dispatcher table size for in-flight RPC requests.
 * This determines how many concurrent RPC handlers can be tracked.
 */
static constexpr size_t MAX_INFLIGHT = 4096;

/**
 * Size of the subscription handler lookup table.
 * Channels are hashed into this table to locate their handler.
 */
static constexpr size_t SUB_TABLE    = 4096;

/**
 * The Dispatcher is responsible for parsing incoming JSON messages and
 * routing them to either RPC callbacks (based on an "id" field) or
 * subscription callbacks (based on a "method" and channel in params).
 *
 * It uses simdjson's ondemand parser to perform zero-copy parsing where
 * possible and stores lightweight views into the original JSON buffer.
 */
class Dispatcher {
    /**
     * Array of RPCHandler entries used for matching responses to
     * previously registered RPC requests. Indexed by id & (MAX_INFLIGHT-1).
     */
    alignas(64) RPCHandler rpc_handler[MAX_INFLIGHT];

    /**
     * Fixed table of subscription handlers. A channel name is hashed and
     * the resulting index is used to look up the handler to call when a
     * notification for that channel is received.
     */
    alignas(64) void (*sub_handler[SUB_TABLE])(const ParsedMessage&) = { nullptr };

    /**
     * simdjson ondemand parser instance used for parsing the incoming
     * JSON message buffer.
     */
    simdjson::ondemand::parser parser;

public:
    Dispatcher() = default;

    /**
     * Register callbacks for an RPC request identifier.
     *
     * @param id The numeric id used to correlate responses with requests.
     * @param on_success Function called when a successful response is received.
     * @param on_error Function called when an error response is received.
     * @param user_data Opaque pointer forwarded to the callbacks.
     */
    inline void register_rpc(uint64_t id,
                             void (*on_success)(const ParsedMessage&, void*),
                             void (*on_error)(const ParsedMessage&, void*),
                             void* user_data)
    {
        RPCHandler& h = rpc_handler[id & (MAX_INFLIGHT - 1)];
        h.on_success = on_success;
        h.on_error   = on_error;
        h.user_data  = user_data;
    }

    /**
     * Register a subscription handler for a channel name.
     *
     * The channel string is hashed into the fixed-size handler table. When
     * a notification for the same channel is received, the stored handler
     * will be invoked with a ParsedMessage describing the notification.
     *
     * @param channel The subscription channel name (string view).
     * @param handler The callback invoked for notifications on this channel.
     */
    inline void register_subscription(std::string_view channel,
                                      void (*handler)(const ParsedMessage&))
    {
        uint32_t idx = fast_hash(channel) & (SUB_TABLE - 1);
        sub_handler[idx] = handler;
    }

    /**
     * Parse and dispatch a single JSON message.
     *
     * The dispatcher inspects the message to determine whether it is an
     * RPC response (contains an "id" field) or a subscription
     * notification (method == "subscription"). For RPC responses the
     * registered RPC handler for the given id will be called. For
     * subscription notifications the channel is extracted from
     * params.channel and the corresponding subscription handler will be
     * invoked.
     *
     * This function performs zero-copy extraction where possible and will
     * return early on malformed messages.
     *
     * @param json Padded JSON message buffer.
     */
    inline void dispatch(simdjson::padded_string_view json) {
        auto doc_res = parser.iterate(json);
        if (doc_res.error() != simdjson::SUCCESS)
            return;

        simdjson::ondemand::document doc = std::move(doc_res.value());

        ParsedMessage pm;

        /* ------------------------------------------------------------
         * Detect RPC responses by presence of an "id" field
         * ------------------------------------------------------------ */
        if (auto id = doc["id"].get_uint64(); id.error() == simdjson::SUCCESS) {
            pm.is_rpc = true;
            pm.id     = id.value();
        }

        /* ------------------------------------------------------------
         * Detect subscription notifications (method == "subscription")
         * ------------------------------------------------------------ */
        if (!pm.is_rpc) {
            std::string_view method_sv;
            if (doc["method"].get(method_sv) == simdjson::SUCCESS &&
                method_sv == "subscription") {
                pm.is_subscription = true;
            }
        }

        /* ------------------------------------------------------------
         * Consume optional latency fields (RPC responses only)
         * ------------------------------------------------------------ */
        if (pm.is_rpc) {
            uint64_t tmp;
            doc["usIn"].get(tmp);
            doc["usOut"].get(tmp);
            doc["usDiff"].get(tmp);
        }
        /* ------------------------------------------------------------
         * Handle RPC response
         * ------------------------------------------------------------ */
        if (pm.is_rpc) {
            RPCHandler& h = rpc_handler[pm.id & (MAX_INFLIGHT - 1)];

            auto err = doc["error"];
            if (err.error() == simdjson::SUCCESS && !err.is_null()) {
                pm.is_error = true;

                int64_t code = 0;
                err["code"].get(code);
                pm.error_code = code;

                std::string_view msg_sv;
                err["message"].get(msg_sv);
                pm.error_msg = msg_sv;

                if (h.on_error)
                    h.on_error(pm, h.user_data);

            } else {
                /* Extract result as raw JSON (zero-copy) */
                auto raw = doc["result"].raw_json();
                if (raw.error() == simdjson::SUCCESS)
                    pm.result = raw.value();

                // Optional: extract access_token if present (used by public/auth)
                auto res_obj = doc["result"].get_object();
                if (res_obj.error() == simdjson::SUCCESS) {
                    auto tok = res_obj.value()["access_token"].get_string();
                    if (tok.error() == simdjson::SUCCESS) {
                        pm.access_token.assign(tok.value());
                    }
                }

                if (h.on_success)
                    h.on_success(pm, h.user_data);
            }
        }

        // Handle subscription notification
        if (pm.is_subscription) {
            auto params_res = doc["params"].get_object();
            if (params_res.error() != simdjson::SUCCESS)
                return;

            simdjson::ondemand::object params = params_res.value();

            auto ch_res = params["channel"].get_string();
            if (ch_res.error() != simdjson::SUCCESS)
                return;

            pm.channel = ch_res.value();

            auto data_res = params["data"].raw_json();
            if (data_res.error() != simdjson::SUCCESS)
                return;

            pm.data = data_res.value();

            const uint32_t idx = fast_hash(pm.channel) & (SUB_TABLE - 1);
            if (const auto handler = sub_handler[idx])
                handler(pm);
        }
    }
};

} // namespace deribit

#endif // HFTDERIBIT_DISPATCHER_H
