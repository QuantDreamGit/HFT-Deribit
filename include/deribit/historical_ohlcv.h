#ifndef HFTDERIBIT_HISTORICAL_OHLCV_H
#define HFTDERIBIT_HISTORICAL_OHLCV_H
#include <vector>
#include <string>
#include <mutex>
#include <condition_variable>

#include <simdjson.h>

#include "ohlcv.h"
#include "parsed_message.h"
#include "deribit_client.h"

namespace deribit {

namespace detail {

    struct OHLCVContext {
        std::vector<OHLCV>* out;
        std::mutex* mtx;
        std::condition_variable* cv;
        bool* done;
    };

    inline void on_ohlcv_success(const ParsedMessage& pm, void* user) {
        auto* ctx = static_cast<detail::OHLCVContext*>(user);

        simdjson::dom::parser parser;
        const simdjson::padded_string json(pm.result.data(), pm.result.size());
        const simdjson::dom::element doc = parser.parse(json);

        // Extract arrays directly from the result
        const simdjson::dom::array close  = doc["close"].get_array();
        const simdjson::dom::array high   = doc["high"].get_array();
        const simdjson::dom::array low    = doc["low"].get_array();
        const simdjson::dom::array open   = doc["open"].get_array();
        const simdjson::dom::array cost  = doc["cost"].get_array();
        const simdjson::dom::array ticks  = doc["ticks"].get_array();
        const simdjson::dom::array volume = doc["volume"].get_array();

        for (int i = 0; i < ticks.size(); i++) {
            OHLCV candle{};
            candle.ts_ms = static_cast<int64_t>(ticks.at(i));
            candle.open  = static_cast<double>(open.at(i));
            candle.high  = static_cast<double>(high.at(i));
            candle.low   = static_cast<double>(low.at(i));
            candle.close = static_cast<double>(close.at(i));
            candle.volume= static_cast<double>(volume.at(i));
            candle.cost  = static_cast<double>(cost.at(i));
            ctx->out->emplace_back(candle);
        }

        // Notify that parsing is done
        {
            std::lock_guard<std::mutex> lk(*ctx->mtx);
            *ctx->done = true;
        }
        ctx->cv->notify_one();
    }

    inline void on_ohlcv_error(const ParsedMessage& pm, void* user) {
        auto* ctx = static_cast<OHLCVContext*>(user);
        {
            std::lock_guard<std::mutex> lk(*ctx->mtx);
            *ctx->done = true;
        }
        ctx->cv->notify_one();
    }
} // namespace detail
    /**
     * @brief Fetch exactly N OHLCV candles for a given instrument and resolution.
     *
     * This function retrieves historical OHLCV data from Deribit in chunks,
     * handling rate limits and ensuring that exactly `n_candles` are returned.
     * It fetches data in reverse chronological order until the desired number
     * of candles is obtained.
     *
     * @param client Reference to an authenticated DeribitClient instance.
     * @param instrument The instrument name (e.g., "BTC-PERPETUAL").
     * @param resolution The candle resolution (e.g., "1", "5", "15", "60", "1D").
     * @param n_candles The total number of candles to fetch.
     * @return A vector of OHLCV structures containing the fetched candle data.
     */
    inline std::vector<OHLCV> fetch_n_ohlcv(
        DeribitClient& client,
        const std::string& instrument,
        const std::string& resolution,
        const size_t n_candles
    ) {
        constexpr size_t CHUNK_SIZE = 1000;

        std::vector<OHLCV> out;
        out.reserve(n_candles + CHUNK_SIZE); // Extra headroom

        std::mutex mtx;
        std::condition_variable cv;
        bool done = false;

        detail::OHLCVContext ctx { &out, &mtx, &cv, &done };

        const std::string res_val = (resolution == "1D") ? "1440" : resolution;
        const int64_t res_ms = std::stoll(res_val) * 60 * 1000;

        int64_t current_end_ts = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        size_t last_size = 0;

        while (out.size() < n_candles) {
            constexpr uint64_t RPC_ID = 0xC0FFEE;
            size_t remaining = n_candles - out.size();
            // Target exactly what is left, or the max chunk size
            const size_t batch_size = std::min(remaining, CHUNK_SIZE);

            // We subtract (batch_size - 1) * res_ms because the window is inclusive.
            // If we want 1000 candles, the window is 999 intervals wide.
            const int64_t current_start_ts = current_end_ts - (static_cast<int64_t>(batch_size - 1) * res_ms);

            {
                std::lock_guard<std::mutex> lk(mtx);
                done = false;
            }

            client.get_dispatcher().register_rpc(
                RPC_ID, &detail::on_ohlcv_success, &detail::on_ohlcv_error, &ctx
            );

            std::string params =
                R"({"instrument_name":")" + instrument + R"(",)"
                R"("resolution":")" += resolution + R"(",)"
                R"("start_timestamp":)" + std::to_string(current_start_ts) + R"(,)"
                R"("end_timestamp":)" + std::to_string(current_end_ts) + "}";

            // If send_rpc returns false, it failed rate limits; we retry.
            if (!client.send_rpc(RPC_ID, "public/get_tradingview_chart_data", params)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                continue;
            }

            {
                std::unique_lock<std::mutex> lk(mtx);
                if (!cv.wait_for(lk, std::chrono::seconds(5), [&]{ return done; })) break;
            }

            if (out.size() == last_size) break;
            last_size = out.size();

            // Move current_end_ts to 1ms BEFORE the current_start_ts to prevent duplicates
            current_end_ts = current_start_ts - 1;
        }

        // Sort chronologically
        std::ranges::sort(out, [](const OHLCV& a, const OHLCV& b) {
            return a.ts_ms < b.ts_ms;
        });

        // Final trim to ensure we have exactly N (removing the oldest if we over-fetched)
        if (out.size() > n_candles) {
            out.erase(out.begin(), out.begin() + static_cast<int64_t>(out.size() - n_candles));
        }

        return out;
    }


} // namespace deribit

#endif //HFTDERIBIT_HISTORICAL_OHLCV_H