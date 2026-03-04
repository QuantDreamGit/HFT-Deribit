#ifndef HFTDERIBIT_FETCH_ORDER_BOOK_H
#define HFTDERIBIT_FETCH_ORDER_BOOK_H

#include <utility>
#include <functional>

#include "../client/deribit_client.h"
#include "../models/oreder_book.h"

using namespace deribit;

namespace deribit {

/**
 * @brief Helper that subscribes and maintains an up-to-date order book for a single instrument.
 *
 * OrderBookFetcher wires into DeribitClient subscriptions and maintains a local OrderBook
 * using snapshot + incremental change messages. It exposes simple callbacks and getters
 * to retrieve top-of-book and full order book state.
 *
 * Responsibilities:
 * - Subscribe to the Deribit realtime book channel (100ms).
 * - Handle first snapshot and subsequent change messages.
 * - Detect missed change messages and resync via a blocking snapshot RPC.
 * - Provide on_update callback notifications and simple getters for top-of-book.
 */
class OrderBookFetcher {
public:
	/**
	 * @brief Construct a fetcher for an instrument.
	 * @param client Reference to the connected DeribitClient used to subscribe and request snapshots.
	 * @param instrument Instrument name (e.g. "BTC-PERPETUAL").
	 * @param depth Depth to request when performing full snapshots (default 10).
	 */
	OrderBookFetcher(DeribitClient& client, std::string instrument, int const depth = 10)
		: client_(&client), instrument_(std::move(instrument)), depth_(depth) {}

	/**
	 * @brief Construct a fetcher with an update callback.
	 * @param client Connected DeribitClient to use.
	 * @param instrument Instrument name.
	 * @param on_update Callback invoked on each update with the current OrderBook snapshot.
	 * @param depth Snapshot depth when resyncing.
	 */
	OrderBookFetcher(DeribitClient& client,
					 std::string instrument,
					 std::function<void(const OrderBook&)> const& on_update,
					 int const depth = 10)
		: client_(&client), instrument_(std::move(instrument)), depth_(depth), on_update_(on_update) {}

	/**
	 * @brief Subscribe to the realtime order book channel and start applying updates.
	 *
	 * The subscription listens to "book.<instrument>.100ms" and expects messages of two types:
	 * - "snapshot": initial full state (applied via handle_snapshot)
	 * - "change": incremental updates (applied via handle_change)
	 *
	 * The callback parses the message data and dispatches to the appropriate handler.
	 */
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

	/**
	 * @brief Handle a snapshot message: replace local book state with the provided payload.
	 *
	 * Sets instrument, timestamp, last_change_id, parses bids/asks and computes top-of-book.
	 * Calls on_update_ if a callback is registered.
	 */
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

	/**
	 * @brief Handle incremental change message and apply operations to the local book.
	 *
	 * Performs a continuity check using prev_change_id and resyncs via snapshot() if messages were missed.
	 * After applying changes it updates timestamps, last_change_id and notifies via on_update_.
	 */
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

	/**
	 * @brief Parse a sequence of price levels from simdjson array into a vector of PriceLevel.
	 * @param arr simdjson array representing levels
	 * @param side destination side vector (bids or asks)
	 */
	static void parse_levels(simdjson::dom::array const arr, std::vector<PriceLevel>& side) {
		side.clear();

		for (auto lvl : arr) {
			PriceLevel pl{};
			pl.price  = static_cast<double>(lvl.at(1));
			pl.amount = static_cast<double>(lvl.at(2));
			side.push_back(pl);
		}
	}

	/**
	 * @brief Apply incremental operations ("new", "change", "delete") to a side vector.
	 *
	 * Each level entry encodes an operation in the first element and price/amount as subsequent elements.
	 */
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

	/**
	 * @brief Compute best bid/ask price and amount from current level vectors.
	 *
	 * Updates book_.best_bid_price/book_.best_bid_amount and best_ask equivalents.
	 */
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

	/**
	 * @brief Set the on-update callback invoked after each applied update.
	 * @tparam Handler callable signature compatible with std::function<void(const OrderBook&)>
	 */
	template<typename Handler>
	void set_on_update(Handler&& h) {
		on_update_ = std::forward<Handler>(h);
	}

	/**
	 * @brief Request a blocking snapshot via the Deribit RPC and build an OrderBook.
	 * @param client Connected DeribitClient for RPCs.
	 * @param instrument Instrument name.
	 * @param depth Desired depth for the snapshot.
	 * @return Constructed OrderBook from RPC result.
	 *
	 * The snapshot method issues a synchronous RPC and parses the returned JSON into OrderBook.
	 */
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
	/** Pointer to the DeribitClient used for subscriptions and RPCs. */
	DeribitClient* client_;
	/** Instrument name this fetcher is maintaining. */
	std::string const instrument_;
	/** Depth to request when performing snapshots. */
	int const depth_;
	/** Local cached OrderBook state. */
	OrderBook book_{};
	/** Last applied change id used to check continuity of updates. */
	uint64_t last_change_id_ = 0;
	/** Optional callback invoked on each update with current OrderBook. */
	std::function<void(const OrderBook&)> on_update_;
};
}

#endif //HFTDERIBIT_FETCH_ORDER_BOOK_H

