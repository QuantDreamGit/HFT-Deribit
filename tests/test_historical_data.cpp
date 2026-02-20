#include "../include/deribit/client/deribit_client.h"
#include "../include/deribit/models/historical_ohlcv.h"
#include "../include/deribit/infra/helpers.h"
#include "../include/deribit/infra/logging.h"

int main() {
   deribit::init_logging();
   deribit::set_log_level(deribit::LogLevel::INFO);

   deribit::DeribitClient client;

   LOG_INFO("Connecting to Deribit...");
   client.connect_sync();

   // Make sure to have a folder
   deribit::helpers::ensure_directory("data");
   deribit::helpers::ensure_directory("data/bin");
   deribit::helpers::ensure_directory("data/csv");

   // Your known instruments
   const std::vector<std::string> instruments = {
      "BTC-PERPETUAL",
      "ETH-PERPETUAL",
      "SOL-PERPETUAL",
      "PAXG-PERPETUAL",
      "XRP-PERPETUAL",
      "TRX-PERPETUAL",
      "AVAX-PERPETUAL",
      "ADA-PERPETUAL",
      "BNB-PERPETUAL",
      "LTC-PERPETUAL",
      "DOGE-PERPETUAL",
      "LINK-PERPETUAL",
      "DOT-PERPETUAL",
      "NEAR-PERPETUAL",
      "TRUMP-PERPETUAL",
      "BCH-PERPETUAL",
      "ALGO-PERPETUAL",
      "UNI-PERPETUAL"
  };

   constexpr size_t N_CANDLES = 60 * 24 * 365 * 10; // 10 years of 1-minute candles
   for (const auto& instrument : instruments) {

      LOG_INFO("Fetching {} candles for {}", N_CANDLES, instrument);

      auto candles = deribit::fetch_n_ohlcv(
          client,
          instrument,
          "1",           // 1-minute resolution
          N_CANDLES
      );

      if (candles.empty()) {
         LOG_WARN("No candles retrieved for {}", instrument);
         continue;
      }

      // Build dynamic filenames
      std::string csv_file = "data/csv/" + instrument + "_1m.csv";
      std::string bin_file = "data/bin/" + instrument + "_1m.bin";

      deribit::helpers::save_to_csv(candles, csv_file);
      deribit::helpers::save_to_bin(candles, bin_file);

      LOG_INFO("Saved {} candles for {}", candles.size(), instrument);
   }

   LOG_INFO("Closing connection...");
   client.close();

   return 0;
}
