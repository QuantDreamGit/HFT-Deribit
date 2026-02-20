#ifndef HFTDERIBIT_HISTORICAL_OHLCV_H
#define HFTDERIBIT_HISTORICAL_OHLCV_H
#include <vector>
#include <string>
#include <mutex>
#include <condition_variable>

#include <simdjson.h>

#include "../models/ohlcv.h"
#include "parsed_message.h"
#include "../client/deribit_client.h"

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

    enum class TradeDirection : uint8_t {
        Buy = 0,
        Sell = 1
    };

    struct alignas(64) Trade {
        int64_t  timestamp;     // ms since epoch
        double   price;
        double   amount;
        uint64_t trade_seq;     // strictly increasing per instrument
        TradeDirection direction;

        // padding to keep 64-byte alignment stable
        std::array<uint8_t, 7> _padding{};
    };
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
        out.reserve(n_candles + CHUNK_SIZE);

        const std::string res_val = (resolution == "1D") ? "1440" : resolution;
        const int64_t res_ms = std::stoll(res_val) * 60 * 1000;

        int64_t current_end_ts =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();

        while (out.size() < n_candles) {

            size_t remaining = n_candles - out.size();
            const size_t batch_size = std::min(remaining, CHUNK_SIZE);

            const int64_t current_start_ts =
                current_end_ts - (static_cast<int64_t>(batch_size - 1) * res_ms);

            std::string params =
                std::string(R"({"instrument_name":")") + instrument +
                R"(","resolution":")" + resolution +
                R"(","start_timestamp":)" + std::to_string(current_start_ts) +
                R"(,"end_timestamp":)" + std::to_string(current_end_ts) +
                "}";

            ParsedMessage pm;

            try {
                pm = client.send_rpc_sync(
                    "public/get_tradingview_chart_data",
                    params,
                    std::chrono::seconds(5)
                );
            }
            catch (const std::exception& e) {
                LOG_ERROR("RPC failed: {}", e.what());
                break;
            }

            if (pm.is_error) {
                LOG_ERROR("OHLCV error: {} {}", pm.error_code, pm.error_msg);
                break;
            }

            if (pm.result.empty()) {
                LOG_WARN("Empty result received");
                break;
            }

            simdjson::dom::parser parser;
            simdjson::padded_string padded(pm.result);

            simdjson::dom::element doc = parser.parse(padded);

            auto close  = doc["close"].get_array();
            auto high   = doc["high"].get_array();
            auto low    = doc["low"].get_array();
            auto open   = doc["open"].get_array();
            auto cost   = doc["cost"].get_array();
            auto ticks  = doc["ticks"].get_array();
            auto volume = doc["volume"].get_array();

            const size_t count = ticks.size();

            if (count == 0) {
                break;
            }

            for (size_t i = 0; i < count; ++i) {
                OHLCV candle{};
                candle.ts_ms = static_cast<int64_t>(ticks.at(i));
                candle.open  = static_cast<double>(open.at(i));
                candle.high  = static_cast<double>(high.at(i));
                candle.low   = static_cast<double>(low.at(i));
                candle.close = static_cast<double>(close.at(i));
                candle.volume= static_cast<double>(volume.at(i));
                candle.cost  = static_cast<double>(cost.at(i));
                out.emplace_back(candle);
            }

            current_end_ts = current_start_ts - 1;
        }

        std::ranges::sort(out, [](const OHLCV& a, const OHLCV& b) {
            return a.ts_ms < b.ts_ms;
        });

        if (out.size() > n_candles) {
            out.erase(out.begin(),
                      out.begin() + static_cast<int64_t>(out.size() - n_candles));
        }

        return out;
    }

    inline std::vector<detail::Trade> fetch_last_n_trades(
    DeribitClient& client,
    const std::string& instrument,
    size_t n_trades)
    {
        constexpr size_t CHUNK_SIZE = 1000;

        std::vector<detail::Trade> out;
        out.reserve(n_trades);

        int64_t current_end_ts =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();

        while (out.size() < n_trades) {

            const size_t remaining = n_trades - out.size();
            const size_t batch_size = std::min(remaining, CHUNK_SIZE);

            std::string params =
                std::string(R"({"instrument_name":")") + instrument +
                R"(","end_timestamp":)" + std::to_string(current_end_ts) +
                R"(,"count":)" + std::to_string(batch_size) +
                R"(,"sorting":"desc"})";

            ParsedMessage pm = client.send_rpc_sync(
                "public/get_last_trades_by_instrument_and_time",
                params
            );

            if (pm.is_error) {
                LOG_ERROR(pm.error_msg);
                return out;
            }

            simdjson::dom::parser parser;
            simdjson::padded_string padded(pm.result);

            simdjson::dom::element doc = parser.parse(padded);

            auto trades = doc["trades"].get_array();

            if (trades.size() == 0)
                break;

            for (auto trade : trades) {
                detail::Trade t{};
                t.timestamp = int64_t(trade["timestamp"]);
                t.price     = double(trade["price"]);
                t.amount    = double(trade["amount"]);
                t.trade_seq = uint64_t(trade["trade_seq"]);

                auto dir = static_cast<std::string>(trade["direction"]);
                t.direction = (dir == "buy") ? detail::TradeDirection::Buy : detail::TradeDirection::Sell;

                out.emplace_back(t);
            }

            // Move backward in time
            current_end_ts =
                int64_t(trades.at(trades.size()-1)["timestamp"]) - 1;
        }

        // We fetched in descending order — reverse to chronological
        std::reverse(out.begin(), out.end());

        return out;
    }





} // namespace deribit

#endif //HFTDERIBIT_HISTORICAL_OHLCV_H