#ifndef HFT_DERIBIT_RATE_LIMITER_H
#define HFT_DERIBIT_RATE_LIMITER_H
#include <chrono>

namespace deribit {

class RateLimiter {
	double tokens;								// Current number of tokens available
	static constexpr double MAX_TOKENS = 20;	// Burst capacity
	static constexpr double REFILL_RATE = 5;	// Tokens per second to refill

	std::chrono::steady_clock::time_point last_refill;;

public:
	RateLimiter() : tokens(MAX_TOKENS),
					last_refill(std::chrono::steady_clock::now()) {};

	bool allow_request() {
		using namespace std::chrono;

		// Check and refill tokens based on elapsed time
		const auto now = steady_clock::now();
		const double elapsed = duration<double> (now - last_refill).count();
		last_refill = now;
		// Refill tokens
		tokens = std::min(MAX_TOKENS, tokens + REFILL_RATE * elapsed);

		if (tokens >= 1.0) {
			tokens -= 1.0;
			return true; // Request allowed
		}

		return false; // Request denied
	}

	double get_tokens() const {
		return tokens;
	}
};
}

#endif //HFT_DERIBIT_RATE_LIMITER_H