#include "deribit/deribit_client.h"
#include "deribit/helpers.h"
#include "deribit/historical_ohlcv.h"
#include "deribit/logging.h"

/**
 * @file main.cpp
 * @brief Utility program to fetch and persist historical OHLCV data from Deribit.
 * * This application demonstrates how to use the DeribitClient to retrieve large
 * datasets of historical candles, format them for console output, and persist
 * them to disk in both human-readable (CSV) and high-performance (Binary) formats.
 */

/**
 * @brief Main entry point for the historical data downloader.
 *
 * The process performs the following steps:
 * 1. Initializes the logging system.
 * 2. Establishes a WebSocket connection to the Deribit API.
 * 3. Fetches a specified number of OHLCV candles (e.g., 20,000) using
 * automated pagination and chunking to stay within API limits.
 * 4. Iterates through the collection to print formatted trade data.
 * 5. Saves the resulting dataset to:
 * - A CSV file for external analysis (Python, Excel).
 * - A Binary file for rapid loading in C++ backtesting environments.
 * 6. Gracefully shuts down the client connection.
 *
 * @return int Exit code 0 on success, non-zero on error.
 */
int main() {
    // Initialize the internal logging framework
    deribit::init_logging();

    /**
     * @brief Client instance for Deribit communication.
     * * Handles the underlying WebSocketBeast connection and RPC dispatching.
     */
    deribit::DeribitClient client;

    LOG_INFO("Connecting to Deribit...");
    client.connect();

    /**
     * @brief Collection of OHLCV candles retrieved from the server.
     * * fetch_n_ohlcv handles the complexity of breaking the 20,000 candle request
     * into manageable chunks of 1,000, ensuring no duplicate candles at
     * chunk boundaries.
     */
    LOG_INFO("Fetching 20,000 candles for BTC-PERPETUAL (60m resolution)...");
    auto candles = deribit::fetch_n_ohlcv(
       client,
       "BTC-PERPETUAL",
       "60",
       20000
    );

    // Print the fetched data to the standard output
    for (const auto& candle : candles) {
       printf(
          "TS: %s, O: %.2f, H: %.2f, L: %.2f, C: %.2f, V: %.4f, Cost: %.2f\n",
          deribit::helpers::print_timestamp(candle.ts_ms).c_str(),
          candle.open,
          candle.high,
          candle.low,
          candle.close,
          candle.volume,
          candle.cost
       );
    }

    /**
     * @brief Persistence logic for historical data.
     * * If data was successfully retrieved, it is saved to disk in two formats.
     */
    if (!candles.empty()) {
       LOG_INFO("Data retrieval complete. Persisting to disk...");

       /** * @brief Export to CSV.
        * Useful for interoperability with data science tools.
        */
       deribit::helpers::save_to_csv(candles, "btc_60m_history.csv");

       /** * @brief Export to Binary.
        * Optimized for C++ alignment (alignas(64)) and zero-parsing load times.
        */
       deribit::helpers::save_to_bin(candles, "btc_60m_history.bin");

       printf("Saved %zu candles to disk.\n", candles.size());
    } else {
       LOG_WARN("No candles were retrieved. Check instrument name or connectivity.");
    }

    // Shut down background threads and close the WebSocket
    LOG_INFO("Closing client connection.");
    client.close();

    return 0;
}