#include <iostream>
#include <fstream>
#include "deribit/client/deribit_client.h"
#include "deribit/infra/fetch_order_book.h"

std::ofstream file("orderbook.csv");

using namespace deribit;

int main() {
	deribit::init_logging();
	SET_LOG_LEVEL(LogLevel::DEBUG);

	DeribitClient client(false);
	client.connect_sync();
	OrderBook book;

	OrderBookFetcher fetcher(client, "BTC-PERPETUAL",
		[&](const OrderBook& ob) {
		auto now = std::chrono::system_clock::now();
		auto local_ts = std::chrono::duration_cast<std::chrono::milliseconds>(
			now.time_since_epoch()).count();

		file << ob.timestamp << "," << local_ts;
		int depth = 100;
		for (size_t i = 0; i < 100; ++i) {
			file << "," << ob.bids[i].price
				 << "," << ob.bids[i].amount;
		}

		for (size_t i = 0; i < 100; ++i) {
			file << "," << ob.asks[i].price
				 << "," << ob.asks[i].amount;
		}

		file << "\n";
	});

	fetcher.subscribe();

	// Wait 10 seconds to collect data
	std::this_thread::sleep_for(std::chrono::minutes(5));

	file.flush();
	file.close();
}