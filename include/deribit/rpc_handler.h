#ifndef HFTDERIBIT_RPC_HANDLER_H
#define HFTDERIBIT_RPC_HANDLER_H
#include "parsed_message.h"

namespace deribit {

/**
 * Small holder for callbacks associated with an in-flight RPC request.
 *
 * The dispatcher stores instances of this struct in a fixed-size table
 * and populates the function pointers when a request is issued. When a
 * response arrives the corresponding handler is looked up and the
 * callbacks are invoked with a ParsedMessage and the provided user_data.
 */
struct RPCHandler {
    /** Callback invoked when the RPC call completed successfully. The
     *  ParsedMessage contains the result field pointing to the raw JSON
     *  result and the user_data pointer is forwarded unchanged. */
    void (*on_success) (const ParsedMessage&, void*) = nullptr;

    /** Callback invoked when the RPC call returned an error. The
     *  ParsedMessage will have is_error set and contain error_code and
     *  error_msg fields; user_data is forwarded unchanged. */
    void (*on_error)   (const ParsedMessage&, void*) = nullptr;

    /** Opaque pointer passed back to callbacks to carry caller state. */
    void* user_data = nullptr;

    /**
     * Clear all callbacks and associated user data.
     *
     * After calling this the handler is effectively inactive.
     */
    inline void clear() {
        on_success = nullptr;
        on_error = nullptr;
        user_data = nullptr;
    }

    /**
     * Check whether this handler has at least one callback set.
     *
     * @return true when either on_success or on_error is non-null.
     */
    [[nodiscard]] inline bool valid() const {
        return on_success || on_error;
    }
};
}

#endif //HFTDERIBIT_RPC_HANDLER_H