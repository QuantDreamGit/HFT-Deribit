#ifndef HFTDERIBIT_FAST_HASH_H
#define HFTDERIBIT_FAST_HASH_H
#include <cstdint>
#include <string_view>

namespace deribit {

/**
 * Compute a fast 32-bit hash for a string view using the FNV-1a algorithm.
 *
 * This function produces a compact hash suitable for indexing small
 * fixed-size tables such as subscription handler arrays. It operates on a
 * std::string_view and performs simple, deterministic mixing of bytes.
 * The implementation is intentionally minimal and inlined for speed.
 *
 * @param sv The input string view to hash.
 * @return A 32-bit hash value.
 */
inline uint32_t fast_hash(const std::string_view sv) {
	uint32_t prime = 2166136261u;
	for (const char c : sv) {
		prime ^= static_cast<uint32_t>(c);
		prime *= 16777619u;
	}

	return prime;
}

} // namespace deribit

#endif //HFTDERIBIT_FAST_HASH_H