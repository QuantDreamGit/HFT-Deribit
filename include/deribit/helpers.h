#ifndef HFTDERIBIT_HELPERS_H
#define HFTDERIBIT_HELPERS_H
#include <chrono>
#include <cstdint>
#include <fstream>
#include <stdexcept>

#include "ohlcv.h"

namespace deribit::helpers {
	/**
	 * @brief Convert a resolution string to milliseconds.
	 *
	 * Supported resolutions:
	 * - "1"   : 1 minute
	 * - "5"   : 5 minutes
	 * - "15"  : 15 minutes
	 * - "60"  : 60 minutes (1 hour)
	 * - "1D"  : 1 day
	 *
	 * @param r Resolution string.
	 * @return Equivalent duration in milliseconds.
	 * @throws std::runtime_error if the resolution is unsupported.
	 */
	inline int64_t resolution_to_ms(const std::string& r) {
		if (r == "1")  return 60'000;
		if (r == "5")  return 5 * 60'000;
		if (r == "15") return 15 * 60'000;
		if (r == "60") return 60 * 60'000;
		if (r == "1D") return 24 * 60 * 60'000;
		throw std::runtime_error("Unsupported resolution");
	}

	/**
	 * @brief Get the current time in milliseconds since epoch.
	 * @return Current time in milliseconds.
	 */
	inline int64_t now_ms() {
		using namespace std::chrono;
		return duration_cast<milliseconds>(
			system_clock::now().time_since_epoch()
		).count();
	}

	/**
	 * @brief Convert a timestamp in milliseconds to a human-readable string.
	 *
	 * The output format is "YYYY-MM-DD HH:MM:SS".
	 *
	 * @param ts_ms Timestamp in milliseconds since epoch.
	 * @return Formatted timestamp string.
	 */
	inline std::string print_timestamp(int64_t ts_ms) {
		// Convert milliseconds to time_point
		const std::chrono::milliseconds ms(ts_ms);
		const std::chrono::system_clock::time_point tp(ms);

		// Convert time_point to system time
		const std::time_t t = std::chrono::system_clock::to_time_t(tp);

		// Convert to tm structure to use it for formatting
		const std::tm* tm_ptr = std::localtime(&t);

		// Return the formatted time as a string
		std::stringstream ss;
		ss << std::put_time(tm_ptr, "%Y-%m-%d %H:%M:%S");
		return ss.str();
	}

	/**
	 * @brief Saves OHLCV data to a CSV file.
	 * Good for Excel, Python/Pandas, and general inspection.
	 */
	inline bool save_to_csv(const std::vector<OHLCV>& candles, const std::string& filename) {
		std::ofstream file(filename);
		if (!file.is_open()) return false;

		// Header
		file << "ts_ms,open,high,low,close,volume,cost\n";

		for (const auto& c : candles) {
			file << c.ts_ms << ","
				 << c.open << ","
				 << c.high << ","
				 << c.low << ","
				 << c.close << ","
				 << c.volume << ","
				 << c.cost << "\n";
		}
		return true;
	}

	/**
	 * @brief Saves OHLCV data as raw binary.
	 * This is the "Best" format for HFT backtesting as it requires
	 * zero parsing logic to load back into memory.
	 */
	inline bool save_to_bin(const std::vector<OHLCV>& candles, const std::string& filename) {
		std::ofstream file(filename, std::ios::binary);
		if (!file.is_open()) return false;

		// Write the count first so we know how much to allocate when loading
		size_t count = candles.size();
		file.write(reinterpret_cast<const char*>(&count), sizeof(size_t));

		// Write the raw buffer
		file.write(reinterpret_cast<const char*>(candles.data()), count * sizeof(OHLCV));
		return true;
	}

	/**
	 * @brief Loads OHLCV data from a binary file.
	 */
	inline std::vector<OHLCV> load_from_bin(const std::string& filename) {
		std::ifstream file(filename, std::ios::binary);
		if (!file.is_open()) return {};

		size_t count = 0;
		file.read(reinterpret_cast<char*>(&count), sizeof(size_t));

		std::vector<OHLCV> candles(count);
		file.read(reinterpret_cast<char*>(candles.data()), count * sizeof(OHLCV));
		return candles;
	}
}


#endif //HFTDERIBIT_HELPERS_H