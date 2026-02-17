#ifndef HFTDERIBIT_ORDER_DETAILS_H
#define HFTDERIBIT_ORDER_DETAILS_H
#include <string>
#include <vector>
#include <optional>
#include <cstdint>

namespace deribit {

// ==========================
// ENUMS
// ==========================

enum class OrderState {
    Open,
    Filled,
    Rejected,
    Cancelled,
    Untriggered,
    Triggered,
    Archive
};

enum class OrderType {
    Market,
    Limit,
    StopMarket,
    StopLimit,
    TakeMarket,
    TakeLimit,
    TrailingStop,
    Liquidation
};

enum class TimeInForce {
    GoodTilCancelled,
    GoodTilDay,
    FillOrKill,
    ImmediateOrCancel
};

enum class Direction {
    Buy,
    Sell
};

enum class TriggerType {
    IndexPrice,
    MarkPrice,
    LastPrice
};

enum class AdvancedType {
    USD,
    ImpliedVol
};

enum class CancelReason {
    UserRequest,
    Autoliquidation,
    CancelOnDisconnect,
    RiskMitigation,
    PMERiskReduction,
    PMEAccountLocked,
    PositionLocked,
    MMPTrigger,
    MMPConfigCurtailment,
    EditPostOnlyReject,
    OCOOtherClosed,
    OTOPrimaryClosed,
    Settlement
};

// ==========================
// TRADE STRUCT
// ==========================

struct TradeDetails {

    std::string trade_id;
    std::string order_id;
    std::string instrument_name;

    std::int64_t timestamp{};
    std::int64_t trade_seq{};

    Direction direction{};
    double price{};
    double amount{};
    double fee{};
    std::string fee_currency;

    double index_price{};
    double mark_price{};

    std::optional<OrderType> order_type;
    std::optional<AdvancedType> advanced;

    std::optional<double> iv;
    std::optional<double> underlying_price;

    std::optional<double> contracts;
    std::optional<double> profit_loss;

    bool api{};
    bool mmp{};
    bool risk_reducing{};
};

// ==========================
// ORDER STRUCT
// ==========================

struct OrderDetails {

    // Core identity
    std::string order_id;
    std::string instrument_name;

    OrderState order_state{};
    OrderType  order_type{};
    Direction  direction{};
    TimeInForce time_in_force{};

    std::int64_t creation_timestamp{};
    std::int64_t last_update_timestamp{};

    double price{};
    double amount{};
    double filled_amount{};
    double average_price{};

    std::optional<double> contracts;

    // Flags
    bool post_only{};
    bool reduce_only{};
    bool api{};
    std::optional<bool> web;
    std::optional<bool> mobile;
    std::optional<bool> is_liquidation;
    std::optional<bool> is_rebalance;

    bool mmp{};
    bool risk_reducing{};
    bool replaced{};
    bool auto_replaced{};
    std::optional<bool> mmp_cancelled;

    // Advanced options fields
    std::optional<AdvancedType> advanced;
    std::optional<double> implv;
    std::optional<double> usd;

    // Trigger fields
    std::optional<bool> triggered;
    std::optional<TriggerType> trigger;
    std::optional<double> trigger_price;
    std::optional<double> trigger_offset;
    std::optional<double> trigger_reference_price;
    std::optional<std::string> trigger_order_id;

    // OCO / OTO
    std::optional<std::vector<std::string>> oto_order_ids;
    std::optional<std::string> oco_ref;
    std::optional<std::string> primary_order_id;
    std::optional<bool> is_secondary_oto;
    std::optional<bool> is_primary_otoco;

    // Misc
    std::optional<std::string> label;
    std::optional<std::string> app_name;
    std::optional<CancelReason> cancel_reason;
    std::optional<bool> quote;
    std::optional<std::string> quote_id;
    std::optional<std::string> quote_set_id;
    std::optional<std::string> mmp_group;

    // Trades attached to order
    std::vector<TradeDetails> trades;
};

} // namespace deribit
#endif //HFTDERIBIT_ORDER_DETAILS_H