#ifndef HFTDERIBIT_CURRENCY_ACCOUNT_STATE_H
#define HFTDERIBIT_CURRENCY_ACCOUNT_STATE_H
#include <string>
#include <unordered_map>
#include <optional>
#include <cstdint>

namespace deribit {

    struct FeeDetail {
        std::string type;   // "relative" or "fixed"
        double taker{};
        double maker{};
        std::optional<double> block_trade;
    };

    using InstrumentFeeMap = std::unordered_map<std::string, FeeDetail>;       // option, future, perpetual
    using IndexFeeMap      = std::unordered_map<std::string, InstrumentFeeMap>; // btc_usd, eth_usd


    struct GreeksSummary {
        double delta{};
        double gamma{};
        double theta{};
        double vega{};
    };

    struct PnLSummary {
        double total_pl{};
        double session_rpl{};
        double session_upl{};
    };

    struct FuturesSummary {
        double pl{};
        double session_rpl{};
        double session_upl{};
    };

    struct OptionsSummary {
        double pl{};
        double session_rpl{};
        double session_upl{};
        double value{};
        GreeksSummary greeks;

        std::unordered_map<std::string, double> gamma_map;
        std::unordered_map<std::string, double> theta_map;
        std::unordered_map<std::string, double> vega_map;
    };

    struct MarginInfo {
        double equity{};
        double balance{};
        double margin_balance{};
        double initial_margin{};
        double maintenance_margin{};
        double projected_initial_margin{};
        double projected_maintenance_margin{};
    };

    struct CrossCollateralUSD {
        std::optional<double> total_equity_usd;
        std::optional<double> total_initial_margin_usd;
        std::optional<double> total_maintenance_margin_usd;
        std::optional<double> total_margin_balance_usd;
        std::optional<double> total_delta_total_usd;
    };

    struct AccountFlags {
        bool security_keys_enabled{};
        bool portfolio_margining_enabled{};
        bool cross_collateral_enabled{};
        bool mmp_enabled{};
        bool interuser_transfers_enabled{};
        bool receive_notifications{};
        std::optional<bool> login_enabled;
    };

    struct AccountIdentity {
        std::string currency;
        std::string username;
        std::string system_name;
        std::string email;
        std::string type;   // main | subaccount
        std::int64_t id{};
        std::int64_t creation_timestamp{};
    };

    struct RiskMetrics {
        double delta_total{};
        double projected_delta_total{};
        double options_delta{};
        std::optional<double> estimated_liquidation_ratio;
    };

    struct BalanceReserves {
        double available_funds{};
        double available_withdrawal_funds{};
        std::optional<double> spot_reserve;
        std::optional<double> additional_reserve;
        std::optional<double> fee_balance;
    };

    /** @brief Comprehensive state of a currency account, including identity, financials, risk metrics, and flags. */
    struct CurrencyAccountState {

        // Identity
        AccountIdentity identity;

        // Financial State
        MarginInfo margin;
        BalanceReserves reserves;
        PnLSummary pnl;
        FuturesSummary futures;
        OptionsSummary options;

        // Risk
        RiskMetrics risk;

        // Cross Collateral Aggregates (USD)
        CrossCollateralUSD cross_usd;

        // Flags
        AccountFlags flags;

        // Fee structure
        std::optional<IndexFeeMap> fees;

        // Misc
        std::optional<std::string> fee_group;
        std::optional<std::string> deposit_address;
        std::optional<std::string> margin_model;
        std::optional<std::string> referrer_id;
        std::optional<double> affiliate_promotion_fee;
        std::optional<bool> has_non_block_chain_equity;

    };

} // namespace deribit

#endif //HFTDERIBIT_CURRENCY_ACCOUNT_STATE_H