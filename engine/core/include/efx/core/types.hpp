#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <compare>
#include <array>
#include <spdlog/fmt/fmt.h>

namespace efx {

using Timestamp = std::chrono::time_point<std::chrono::steady_clock>;
using WallClock = std::chrono::time_point<std::chrono::system_clock>;
using Price     = double;
using Amount    = double;

enum class Side : uint8_t { Buy, Sell };

constexpr std::string_view to_string(Side s) {
    return s == Side::Buy ? "BUY" : "SELL";
}

constexpr Side opposite(Side s) {
    return s == Side::Buy ? Side::Sell : Side::Buy;
}

struct VenueId {
    std::string id;
    auto operator<=>(const VenueId&) const = default;
};

struct ClientId {
    std::string id;
    auto operator<=>(const ClientId&) const = default;
};

struct CurrencyPair {
    std::string symbol;     // e.g. "EURUSD"
    double pip_size;        // e.g. 0.00001 for EURUSD, 0.001 for USDJPY
    double tick_size;

    auto operator<=>(const CurrencyPair&) const = default;

    std::string_view base() const { return std::string_view(symbol).substr(0, 3); }
    std::string_view quote() const { return std::string_view(symbol).substr(3, 3); }

    double to_pips(double price_diff) const { return price_diff / pip_size; }
    double from_pips(double pips) const { return pips * pip_size; }
};

struct VenueTick {
    VenueId venue;
    std::string pair;
    Price bid;
    Price ask;
    Amount bid_size;
    Amount ask_size;
    Timestamp timestamp;
    WallClock wall_time;

    Price mid() const { return (bid + ask) / 2.0; }
    double spread() const { return ask - bid; }
};

struct FairValue {
    std::string pair;
    Price mid;
    double confidence;
    double volatility;
    int num_active_venues;
    Timestamp timestamp;
    WallClock wall_time;
};

struct ClientPrice {
    ClientId client_id;
    std::string pair;
    Price bid;
    Price ask;
    double spread_bps;
    uint32_t valid_for_ms;
    uint64_t quote_id;
    Timestamp timestamp;
    WallClock wall_time;
};

struct Fill {
    uint64_t trade_id;
    uint64_t quote_id;
    ClientId client_id;
    std::string pair;
    Side side;
    Price price;
    Amount amount;
    VenueId venue;
    Price mid_at_fill;
    double spread_bps;
    std::string session;
    Timestamp timestamp;
    WallClock wall_time;
};

struct HedgeExecution {
    uint64_t hedge_id;
    uint64_t triggering_trade_id;
    std::string pair;
    Side side;
    Price price;
    Amount amount;
    VenueId venue;
    double slippage_bps;
    uint64_t latency_us;
    Timestamp timestamp;
    WallClock wall_time;
};

enum class TierId : uint8_t {
    Platinum = 0,
    Gold = 1,
    Silver = 2,
    Bronze = 3,
    Restricted = 4,
};

constexpr std::array<double, 5> TIER_MULTIPLIERS = {1.0, 1.5, 2.5, 4.0, 8.0};

constexpr double tier_multiplier(TierId tier) {
    return TIER_MULTIPLIERS[static_cast<uint8_t>(tier)];
}

constexpr std::string_view tier_name(TierId tier) {
    constexpr std::array<std::string_view, 5> names = {
        "platinum", "gold", "silver", "bronze", "restricted"
    };
    return names[static_cast<uint8_t>(tier)];
}

enum class Toxicity : uint8_t { Low, Medium, High, Extreme };

constexpr std::string_view to_string(Toxicity t) {
    constexpr std::array<std::string_view, 4> names = {"low", "medium", "high", "extreme"};
    return names[static_cast<uint8_t>(t)];
}

enum class SessionId : uint8_t { NewYork, Singapore, London };

constexpr std::string_view to_string(SessionId s) {
    constexpr std::array<std::string_view, 3> names = {"new_york", "singapore", "london"};
    return names[static_cast<uint8_t>(s)];
}

struct PnlSnapshot {
    std::string pair;
    std::string session;
    double spread_pnl;
    double position_pnl;
    double hedge_cost;
    double total_pnl;
    double position;
    double drawdown;
    double peak_pnl;
    WallClock wall_time;
};

} // namespace efx
