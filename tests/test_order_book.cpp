#include <iostream>
#include <fstream>
#include "deribit/client/deribit_client.h"
#include "deribit/infra/fetch_order_book.h"

/// Output CSV file for captured order book snapshots (timestamp + top levels).
std::ofstream file("orderbook.csv");

using namespace deribit;

/**
 * @brief Simple utility that subscribes to BTC-PERPETUAL order book updates
 * and writes snapshots into a CSV file for offline analysis.
 *
 * CSV layout:
 *   ob.timestamp, local_ts,
 *   bids[0].price, bids[0].amount, ..., bids[99].price, bids[99].amount,
 *   asks[0].price, asks[0].amount, ..., asks[99].price, asks[99].amount
 *
 * The program:
 * - initializes logging,
 * - constructs and connects a DeribitClient synchronously,
 * - creates an OrderBookFetcher with a callback that writes CSV rows,
 * - subscribes to realtime updates and waits to collect data,
 * - flushes and closes the CSV file before exit.
 */
int main() {
	// Initialize logging utilities and set debug level.
	deribit::init_logging();
	SET_LOG_LEVEL(LogLevel::DEBUG);

	// Create a client (mainnet=false -> mainnet) and connect synchronously.
	DeribitClient client(false);
	client.connect_sync();
	OrderBook book;

	// Create an OrderBookFetcher that writes each update into the CSV file.
	// The callback records both the remote timestamp (ob.timestamp) and a local timestamp,
	// followed by 100 bids and 100 asks (price,amount pairs).
	OrderBookFetcher fetcher(client, "BTC-PERPETUAL",
		[&](const OrderBook& ob) {
		// Local timestamp in milliseconds for correlation/debugging.
		auto now = std::chrono::system_clock::now();
		auto local_ts = std::chrono::duration_cast<std::chrono::milliseconds>(
			now.time_since_epoch()).count();

		// Write CSV: remote timestamp, local timestamp
		file << ob.timestamp << "," << local_ts;
		int depth = 100;
		// Write top 100 bids (price,amount)
		for (size_t i = 0; i < 100; ++i) {
			file << "," << ob.bids[i].price
				 << "," << ob.bids[i].amount;
		}

		// Write top 100 asks (price,amount)
		for (size_t i = 0; i < 100; ++i) {
			file << "," << ob.asks[i].price
				 << "," << ob.asks[i].amount;
		}

		file << "\n";
	});

	// Start receiving realtime updates for the instrument.
	fetcher.subscribe();

	// Wait to collect data.
	std::this_thread::sleep_for(std::chrono::minutes(5));

	// Ensure all buffered output is written and close the file.
	file.flush();
	file.close();
}