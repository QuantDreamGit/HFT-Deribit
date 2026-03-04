#ifndef HFTDERIBIT_OREDER_BOOK_H
#define HFTDERIBIT_OREDER_BOOK_H
#include <string>
#include <vector>
#include <cstdint>

namespace deribit {
	struct PriceLevel {
		double price;
		double amount;
	};

	struct OrderBook {
		std::string instrument;

		std::uint64_t timestamp;
		std::uint64_t change_id;

		double mark_price;
		double index_price;
		double last_price;

		double best_bid_price;
		double best_bid_amount;

		double best_ask_price;
		double best_ask_amount;

		double funding_8h;
		double current_funding;

		double open_interest;

		std::vector<PriceLevel> bids;
		std::vector<PriceLevel> asks;
	};

}

#endif //HFTDERIBIT_OREDER_BOOK_H