#ifndef HFTDERIBIT_OREDER_BOOK_H
#define HFTDERIBIT_OREDER_BOOK_H
#include <string>
#include <vector>
#include <cstdint>

namespace deribit {

    /**
     * @brief Represents a single price level in an order book.
     *
     * Each PriceLevel contains a price and the amount available at that price.
     */
    struct PriceLevel {
        double price;   /**< Price at this level */
        double amount;  /**< Quantity available at this price */
    };

    /**
     * @brief Lightweight snapshot of an instrument's order book and related metadata.
     *
     * OrderBook holds basic market metadata (prices, funding, open interest)
     * and vectors of bids and asks represented as PriceLevel entries.
     */
    struct OrderBook {
        std::string instrument; /**< Instrument identifier (e.g., "BTC-PERPETUAL") */

        std::uint64_t timestamp; /**< Epoch timestamp (ms) of the snapshot or update */
        std::uint64_t change_id; /**< Change identifier used to order incremental updates */

        double mark_price;   /**< Current mark price */
        double index_price;  /**< Reference/index price */
        double last_price;   /**< Last traded price */

        double best_bid_price;   /**< Best bid price (top of bids) */
        double best_bid_amount;  /**< Amount available at best bid */

        double best_ask_price;   /**< Best ask price (top of asks) */
        double best_ask_amount;  /**< Amount available at best ask */

        double funding_8h;      /**< 8-hour funding estimate */
        double current_funding; /**< Current funding rate */

        double open_interest; /**< Open interest for the instrument */

        std::vector<PriceLevel> bids; /**< Bid side price levels (unsorted or sorted depending on source) */
        std::vector<PriceLevel> asks; /**< Ask side price levels (unsorted or sorted depending on source) */
    };

}

#endif //HFTDERIBIT_OREDER_BOOK_H