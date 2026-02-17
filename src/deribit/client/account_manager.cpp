#include "deribit/client/account_manager.h"
#include "deribit/client/deribit_client.h"
#include "deribit/models/parsed_message.h"
#include "deribit/infra/logging.h"

#include <simdjson.h>

namespace deribit {

void AccountManager::initialize(DeribitClient& client) {

    auto on_success = [](const ParsedMessage& pm, void* user_ptr) {
        auto* self = static_cast<AccountManager*>(user_ptr);
        self->initialize_from_snapshot(pm.result);
        LOG_INFO("Account summaries updated successfully");
    };

    auto on_error = [](const ParsedMessage& pm, void* user_ptr) {
        LOG_ERROR("Failed to get account summaries: {}", pm.error_msg);
    };

    client.set_rpc_dispatch_handler(12345, on_success, on_error, this);

    client.send_rpc(
        12345,
        "private/get_account_summaries",
        R"({"extended":"true"})"
    );
}

void AccountManager::initialize_from_snapshot(const std::string& json_snapshot) {

    simdjson::dom::parser parser;
    simdjson::padded_string padded(json_snapshot);

    auto doc_res = parser.parse(padded);
    if (doc_res.error()) {
        LOG_ERROR("Parse error: {}", simdjson::error_message(doc_res.error()));
        return;
    }

    auto doc = doc_res.value();

    auto summaries_res = doc["summaries"].get_array();
    if (summaries_res.error()) {
        LOG_ERROR("No summaries field in snapshot");
        return;
    }

    auto summaries = summaries_res.value();

    std::lock_guard<std::mutex> lock(mtx_);
    accounts_.clear();

    for (auto summary : summaries) {
        CurrencyAccountState state;

        std::string currency = std::string(summary["currency"]);

        state.equity             = double(summary["equity"]);
        state.balance            = double(summary["balance"]);
        state.available_funds    = double(summary["available_funds"]);
        state.margin_balance     = double(summary["margin_balance"]);
        state.initial_margin     = double(summary["initial_margin"]);
        state.maintenance_margin = double(summary["maintenance_margin"]);
        state.unrealized_pnl     = double(summary["session_upl"]);
        state.realized_pnl       = double(summary["session_rpl"]);
        state.total_delta        = double(summary["delta_total"]);
        state.liquidation_ratio  = double(summary["estimated_liquidation_ratio"]);

        accounts_[currency] = state;
    }
}

CurrencyAccountState AccountManager::get_state(const std::string& currency) const {
    std::lock_guard<std::mutex> lock(mtx_);

    auto it = accounts_.find(currency);
    if (it == accounts_.end())
        return {};

    return it->second;
}

} // namespace deribit
