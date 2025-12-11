#ifndef HFT_DERIBIT_RATE_LIMITER_H
#define HFT_DERIBIT_RATE_LIMITER_H
#include <chrono>
#include <algorithm>

namespace deribit {

/**
 * Simple token bucket rate limiter.
 *
 * The limiter tracks a floating point token balance that is refilled at a
 * constant rate. Each allowed request consumes one token. The bucket has a
 * maximum burst capacity so tokens cannot accumulate indefinitely.
 */
class RateLimiter {
    /** Current number of tokens available for immediate requests. */
    double tokens;

    /** Maximum number of tokens the bucket can hold (burst capacity). */
    static constexpr double MAX_TOKENS = 20;

    /** Refill rate expressed in tokens per second. */
    static constexpr double REFILL_RATE = 5;

    /** Time point of the last refill operation used to compute elapsed time. */
    std::chrono::steady_clock::time_point last_refill;

public:
    /**
     * Construct a rate limiter starting full.
     */
    RateLimiter() : tokens(MAX_TOKENS),
                    last_refill(std::chrono::steady_clock::now()) {}

    /**
     * Attempt to allow a single request.
     *
     * This updates the token balance based on elapsed wall-clock time since
     * the previous call, refilling up to MAX_TOKENS. If at least one token
     * is available after the refill, one token is consumed and the function
     * returns true. Otherwise it returns false to indicate the request is
     * rate limited.
     *
     * @return true when the request is allowed, false when denied.
     */
    bool allow_request() {
        using namespace std::chrono;

        // Check and refill tokens based on elapsed time
        const auto now = steady_clock::now();
        const double elapsed = duration<double>(now - last_refill).count();
        last_refill = now;

        // Refill tokens up to the configured maximum
        tokens = std::min(MAX_TOKENS, tokens + REFILL_RATE * elapsed);

        if (tokens >= 1.0) {
            tokens -= 1.0;
            return true; // Request allowed
        }

        return false; // Request denied
    }

    /**
     * Query the current token balance.
     *
     * This returns the internal floating point token count and is primarily
     * useful for diagnostics and testing; callers should not assume the
     * value remains stable in concurrent contexts.
     */
    double get_tokens() const {
        return tokens;
    }
};
} // namespace deribit

#endif //HFT_DERIBIT_RATE_LIMITER_H