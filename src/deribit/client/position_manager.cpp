#include "deribit/client/position_manager.h"
#include <simdjson.h>
#include <stdexcept>

namespace deribit {

PositionManager::PositionManager(DeribitClient& client)
    : client_(&client)
{}

OrderDetails PositionManager::market_buy(
    const std::string& instrument,
    double usd_amount,
    const std::string& margin_currency,
    const Direction side
) {
    // 1. Get current account state
    CurrencyAccountState state =
        client_->account_manager.get_currency_state(margin_currency);

    const double available = state.reserves.available_funds;

    if (available <= 0.0) {
        LOG_ERROR("No available funds");
        return OrderDetails{};
    }

    if (usd_amount > available) {
        LOG_ERROR("Insufficient available funds");
        return OrderDetails{};
    }

    // 2. Build RPC params
    const std::string method = (side == Direction::Buy) ? "private/buy" : "private/sell";

    std::string params =
        std::string(R"({"instrument_name":")") + instrument +
        R"(","amount":)" + std::to_string(usd_amount) +
        R"(,"type":"market"})";

    ParsedMessage pm =
        client_->send_rpc_sync(method, params);

    if (pm.is_error) {
        LOG_ERROR("Buy failed: ", pm.error_msg);
    }

    // 4. Parse order response
    return parse_order_from_result(pm.result);
}

OrderDetails PositionManager::parse_order_from_result(
    const std::string& result_json
) {
    simdjson::dom::parser parser;
    auto doc = parser.parse(result_json);

    simdjson::dom::element order_elem;

    // Compatible with both:
    // result.order {...}
    // result {...}
    if (doc["order"].error() == simdjson::SUCCESS) {
        order_elem = doc["order"];
    }

    OrderDetails order;

    order.order_id = std::string(order_elem["order_id"]);
    order.instrument_name = std::string(order_elem["instrument_name"]);

    order.price = double(order_elem["price"]);
    order.amount = double(order_elem["amount"]);
    order.filled_amount = double(order_elem["filled_amount"]);
    order.average_price = double(order_elem["average_price"]);

    order.creation_timestamp =
        std::int64_t(order_elem["creation_timestamp"]);

    order.last_update_timestamp =
        std::int64_t(order_elem["last_update_timestamp"]);

    return order;
}

} // namespace deribit
