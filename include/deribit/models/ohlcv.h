#ifndef HFTDERIBIT_OHLCV_H
#define HFTDERIBIT_OHLCV_H
#include <cstdint>

namespace deribit {

	struct alignas(64) OHLCV {
		int64_t ts_ms;
		double open;
		double high;
		double low;
		double close;
		double volume;
		double cost;
	};

} // namespace deribit

#endif //HFTDERIBIT_OHLCV_H