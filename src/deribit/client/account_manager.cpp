#include "deribit/client/account_manager.h"
#include "deribit/client/deribit_client.h"
#include "deribit/models/parsed_message.h"
#include "deribit/infra/logging.h"
#include "deribit/models/currency_account_state.h"

#include <simdjson.h>

namespace deribit {

void AccountManager::initialize(DeribitClient& client) {
    // Store pointer to client for future RPC calls
    client_ = &client;

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
        AccountState state;

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

CurrencyAccountState AccountManager::get_currency_state(const std::string& currency) const {

    ParsedMessage msg = client_->send_rpc_sync(
        "private/get_account_summary",
        R"({"currency":")" + currency + R"("})"
    );

    CurrencyAccountState state;

    simdjson::dom::parser parser;
    auto doc = parser.parse(msg.result);

    // -----------------------
    // Identity
    // -----------------------
    state.identity.currency = std::string(doc["currency"]);

    // -----------------------
    // Margin Info
    // -----------------------
    state.margin.equity                      = double(doc["equity"]);
    state.margin.balance                     = double(doc["balance"]);
    state.margin.margin_balance              = double(doc["margin_balance"]);
    state.margin.initial_margin              = double(doc["initial_margin"]);
    state.margin.maintenance_margin          = double(doc["maintenance_margin"]);
    state.margin.projected_initial_margin    = double(doc["projected_initial_margin"]);
    state.margin.projected_maintenance_margin= double(doc["projected_maintenance_margin"]);

    // -----------------------
    // Balance Reserves
    // -----------------------
    state.reserves.available_funds            = double(doc["available_funds"]);
    state.reserves.available_withdrawal_funds = double(doc["available_withdrawal_funds"]);

    if (auto v = doc["spot_reserve"]; !v.is_null())
        state.reserves.spot_reserve = double(v);

    if (auto v = doc["additional_reserve"]; !v.is_null())
        state.reserves.additional_reserve = double(v);

    if (auto v = doc["fee_balance"]; !v.is_null())
        state.reserves.fee_balance = double(v);

    // -----------------------
    // PnL
    // -----------------------
    state.pnl.total_pl     = double(doc["total_pl"]);
    state.pnl.session_rpl  = double(doc["session_rpl"]);
    state.pnl.session_upl  = double(doc["session_upl"]);

    // -----------------------
    // Futures
    // -----------------------
    state.futures.pl           = double(doc["futures_pl"]);
    state.futures.session_rpl  = double(doc["futures_session_rpl"]);
    state.futures.session_upl  = double(doc["futures_session_upl"]);

    // -----------------------
    // Options
    // -----------------------
    state.options.pl           = double(doc["options_pl"]);
    state.options.session_rpl  = double(doc["options_session_rpl"]);
    state.options.session_upl  = double(doc["options_session_upl"]);
    state.options.value        = double(doc["options_value"]);

    state.options.greeks.delta = double(doc["options_delta"]);
    state.options.greeks.gamma = double(doc["options_gamma"]);
    state.options.greeks.theta = double(doc["options_theta"]);
    state.options.greeks.vega  = double(doc["options_vega"]);

    // Maps
    if (auto gamma_map = doc["options_gamma_map"].get_object(); gamma_map.error() == simdjson::SUCCESS) {
        for (auto field : gamma_map.value()) {
            state.options.gamma_map[std::string(field.key)] = double(field.value);
        }
    }

    if (auto theta_map = doc["options_theta_map"].get_object(); theta_map.error() == simdjson::SUCCESS) {
        for (auto field : theta_map.value()) {
            state.options.theta_map[std::string(field.key)] = double(field.value);
        }
    }

    if (auto vega_map = doc["options_vega_map"].get_object(); vega_map.error() == simdjson::SUCCESS) {
        for (auto field : vega_map.value()) {
            state.options.vega_map[std::string(field.key)] = double(field.value);
        }
    }

    // -----------------------
    // Risk
    // -----------------------
    state.risk.delta_total          = double(doc["delta_total"]);
    state.risk.projected_delta_total= double(doc["projected_delta_total"]);
    state.risk.options_delta        = double(doc["options_delta"]);

    if (auto v = doc["estimated_liquidation_ratio"]; !v.is_null())
        state.risk.estimated_liquidation_ratio = double(v);

    // -----------------------
    // Flags
    // -----------------------
    state.flags.portfolio_margining_enabled = bool(doc["portfolio_margining_enabled"]);
    state.flags.cross_collateral_enabled    = bool(doc["cross_collateral_enabled"]);

    // -----------------------
    // Margin model
    // -----------------------
    if (auto v = doc["margin_model"]; !v.is_null())
        state.margin_model = std::string(v);

    return state;
}

AccountState AccountManager::get_state(const std::string& currency) const {
    std::lock_guard<std::mutex> lock(mtx_);

    auto it = accounts_.find(currency);
    if (it == accounts_.end())
        return {};

    return it->second;
}

} // namespace deribit
