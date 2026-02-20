#include "../include/deribit/client/deribit_client.h"
#include "../include/deribit/infra/helpers.h"
#include "../include/deribit/infra/logging.h"
#include "deribit/models/historical_ohlcv.h"

int main() {

	deribit::init_logging();

	deribit::DeribitClient client;

	LOG_INFO("Connecting to Deribit...");
	client.connect_sync();

	constexpr size_t N_TRADES = 50000;

	LOG_INFO("Fetching last {} trades for BTC-PERPETUAL...", N_TRADES);

	auto trades = deribit::fetch_last_n_trades(
		client,
		"BTC-PERPETUAL",
		N_TRADES
	);

	for (const auto& trade : trades) {

		const char* dir =
			(trade.direction == deribit::detail::TradeDirection::Buy)
			? "BUY"
			: "SELL";

		printf(
			"TS: %s | Seq: %llu | Price: %.2f | Amount: %.6f | Dir: %s\n",
			deribit::helpers::print_timestamp(trade.timestamp).c_str(),
			static_cast<unsigned long long>(trade.trade_seq),
			trade.price,
			trade.amount,
			dir
		);
	}

	if (!trades.empty()) {

		LOG_INFO("Persisting trades to disk...");

		deribit::helpers::save_trades_to_csv(
			trades,
			"btc_trades.csv"
		);

		printf("Saved %zu trades to disk.\n", trades.size());
	}
	else {
		LOG_WARN("No trades were retrieved.");
	}

	LOG_INFO("Closing client connection.");
	client.close();

	return 0;
}
