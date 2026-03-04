#ifndef HFTDERIBIT_FETCH_ORDER_BOOK_H
#define HFTDERIBIT_FETCH_ORDER_BOOK_H

#include <utility>
#include <functional>

#include "../client/deribit_client.h"
#include "../models/oreder_book.h"

using namespace deribit;

namespace deribit {
	class OrderBookFetcher {
	public:
		OrderBookFetcher(DeribitClient& client, std::string instrument, int const depth = 10)
			: client_(&client), instrument_(std::move(instrument)), depth_(depth) {}

		OrderBookFetcher(DeribitClient& client,
						 std::string instrument,
						 std::function<void(const OrderBook&)> const& on_update,
						 int const depth = 10)
			: client_(&client), instrument_(std::move(instrument)), depth_(depth), on_update_(on_update) {}

		void subscribe() {
			client_->subscribe(
				"book." + instrument_ + ".100ms",
				[this](const ParsedMessage& pm) {
				// Handle incoming order book update
				// There are 2 types: snapshot (only first message) and change
				// Also each message contains a change_id which is used to track the order of messages and ensure
				// that no updates are missed.
				simdjson::dom::parser parser;
				auto const data = parser.parse(pm.data).value();
				std::string_view type = data["type"];
				// Handle snapshot and change messages
				if (type == "snapshot") {
					handle_snapshot(data);
				}
				else if (type == "change") {
					handle_change(data);
				}
			});
		}

		void handle_snapshot(simdjson::dom::element data) {
			book_.instrument = instrument_;
			book_.timestamp = static_cast<uint64_t>(data["timestamp"]);
			last_change_id_ = static_cast<uint64_t>(data["change_id"]);
			parse_levels(data["bids"], book_.bids);
			parse_levels(data["asks"], book_.asks);
			// Compute bid and ask price/amount
			compute_top_of_book();
			// Call update callback if set
			if (on_update_) on_update_(book_);
		}

		void handle_change(simdjson::dom::element data) {
			uint64_t prev_change = static_cast<uint64_t>(data["prev_change_id"]);
			uint64_t change_id   = static_cast<uint64_t>(data["change_id"]);

			// Detect missed messages
			if (prev_change != last_change_id_) {
				// resync orderbook
				book_ = snapshot(*client_, instrument_, depth_);
				last_change_id_ = change_id;
				return;
			}

			apply_changes(data["bids"], book_.bids);
			apply_changes(data["asks"], book_.asks);
			book_.timestamp = static_cast<uint64_t>(data["timestamp"]);
			last_change_id_ = change_id;
			// Compute bid and ask price/amount
			compute_top_of_book();
			// Call update callback if set
			if (on_update_) on_update_(book_);
		}

		static void parse_levels(simdjson::dom::array const arr, std::vector<PriceLevel>& side) {
			side.clear();

			for (auto lvl : arr) {
				PriceLevel pl{};
				pl.price  = static_cast<double>(lvl.at(1));
				pl.amount = static_cast<double>(lvl.at(2));
				side.push_back(pl);
			}
		}

		static void apply_changes(simdjson::dom::array arr, std::vector<PriceLevel>& side) {
			for (auto lvl : arr) {
				std::string_view const op = lvl.at(0);

				double price  = static_cast<double>(lvl.at(1));
				double const amount = static_cast<double>(lvl.at(2));

				if (op == "new") {

					side.push_back({price, amount});

				} else if (op == "change") {

					for (auto& p : side) {
						if (p.price == price) {
							p.amount = amount;
							break;
						}
					}

				} else if (op == "delete") {

					side.erase(
						std::remove_if(
							side.begin(),
							side.end(),
							[price](const PriceLevel& p) {
								return p.price == price;
							}),
						side.end());
				}
			}
		}

		void compute_top_of_book()
		{
			if (!book_.bids.empty()) {

				auto best = std::max_element(
					book_.bids.begin(),
					book_.bids.end(),
					[](const PriceLevel& a, const PriceLevel& b)
					{
						return a.price < b.price;
					});

				book_.best_bid_price  = best->price;
				book_.best_bid_amount = best->amount;
			}

			if (!book_.asks.empty()) {

				auto best = std::min_element(
					book_.asks.begin(),
					book_.asks.end(),
					[](const PriceLevel& a, const PriceLevel& b)
					{
						return a.price < b.price;
					});

				book_.best_ask_price  = best->price;
				book_.best_ask_amount = best->amount;
			}
		}

		template<typename Handler>
		void set_on_update(Handler&& h) {
			on_update_ = std::forward<Handler>(h);
		}

		static OrderBook snapshot(DeribitClient& client, std::string const& instrument, int const depth = 10) {
			// Param creation
			std::string params;
			params.reserve(64);

			params += R"({"instrument_name":")";
			params += instrument;
			params += R"(","depth":)";
			params += std::to_string(depth);
			params += "}";

			// Get parsed message
			ParsedMessage pm = client.send_rpc_sync("public/get_order_book", params);

			// Parse JSON
			simdjson::dom::parser parser;
			auto result = parser.parse(pm.result);
			OrderBook book{};
			book.instrument = instrument;

			// Basic metadata
			book.timestamp       = static_cast<uint64_t>(result["timestamp"]);
			book.mark_price      = static_cast<double>(result["mark_price"]);
			book.index_price     = static_cast<double>(result["index_price"]);
			book.last_price      = static_cast<double>(result["last_price"]);

			book.best_bid_price  = static_cast<double>(result["best_bid_price"]);
			book.best_bid_amount = static_cast<double>(result["best_bid_amount"]);

			book.best_ask_price  = static_cast<double>(result["best_ask_price"]);
			book.best_ask_amount = static_cast<double>(result["best_ask_amount"]);

			book.funding_8h      = static_cast<double>(result["funding_8h"]);
			book.current_funding = static_cast<double>(result["current_funding"]);

			book.open_interest   = static_cast<double>(result["open_interest"]);

			// Reserve space to avoid reallocations
			book.bids.reserve(depth);
			book.asks.reserve(depth);

			// Parse bids
			for (auto bid : result["bids"]) {
				PriceLevel lvl{};
				lvl.price  = static_cast<double>(bid.at(0));
				lvl.amount = static_cast<double>(bid.at(1));
				book.bids.push_back(lvl);
			}

			// Parse asks
			for (auto ask : result["asks"]) {
				PriceLevel lvl{};
				lvl.price  = static_cast<double>(ask.at(0));
				lvl.amount = static_cast<double>(ask.at(1));
				book.asks.push_back(lvl);
			}

			return book;
		}

		// Getters
		[[nodiscard]] double getBidPrice() const { return book_.best_bid_price; }
		[[nodiscard]] double getAskPrice() const { return book_.best_ask_price; }
		[[nodiscard]] double getBidAmount() const { return book_.best_bid_amount; }
		[[nodiscard]] double getAskAmount() const { return book_.best_ask_amount; }
		[[nodiscard]] OrderBook getOrderBook() const { return book_; }

	private:
		DeribitClient* client_;
		std::string const instrument_;
		int const depth_;
		OrderBook book_{};
		uint64_t last_change_id_ = 0;
		std::function<void(const OrderBook&)> on_update_;
	};
}



#endif //HFTDERIBIT_FETCH_ORDER_BOOK_H