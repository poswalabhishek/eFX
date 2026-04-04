#pragma once

#include "efx/core/types.hpp"
#include "efx/core/config.hpp"

#include <string>
#include <unordered_map>
#include <vector>
#include <deque>
#include <mutex>
#include <atomic>
#include <functional>
#include <chrono>

namespace efx {

struct Order {
    uint64_t order_id;
    ClientId client_id;
    std::string pair;
    Side side;
    Amount amount;
    Price limit_price;   // for market orders, this is the quoted price
    uint64_t quote_id;
    VenueId venue;
    Timestamp created_at;
    WallClock wall_time;
};

enum class FillStatus : uint8_t { Filled, Rejected, Expired };

struct FillResult {
    FillStatus status;
    Fill fill;           // only valid if status == Filled
    std::string reject_reason;
};

// Tracks positions per currency pair with real-time mark-to-market.
class PositionManager {
public:
    struct PairPosition {
        std::string pair;
        double position = 0.0;      // positive = long, negative = short
        double avg_entry_price = 0.0;
        double realized_pnl = 0.0;
        double unrealized_pnl = 0.0;
        double last_mid = 0.0;
        int trade_count = 0;
    };

    void on_fill(const Fill& fill);
    void mark_to_market(const std::string& pair, double mid);

    const PairPosition* get_position(const std::string& pair) const;
    std::vector<PairPosition> get_all_positions() const;
    double total_realized_pnl() const;
    double total_unrealized_pnl() const;
    double net_position_usd(const std::string& pair) const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, PairPosition> positions_;
};

// Core order management: validates orders and executes fills.
class OrderManager {
public:
    using FillCallback = std::function<void(const Fill&)>;

    explicit OrderManager(const AppConfig& config, PositionManager& pos_mgr);

    void set_fill_callback(FillCallback cb) { fill_callback_ = std::move(cb); }

    // Submit an order for execution against a quoted price.
    FillResult submit_order(const Order& order, Price current_mid);

    // Get recent fills (thread-safe snapshot)
    std::vector<Fill> recent_fills(size_t limit = 50) const;

    uint64_t next_trade_id() { return trade_id_counter_.fetch_add(1); }
    uint64_t next_order_id() { return order_id_counter_.fetch_add(1); }
    uint64_t total_fills() const { return total_fills_.load(); }
    uint64_t total_rejects() const { return total_rejects_.load(); }

private:
    bool validate_order(const Order& order, std::string& reason) const;

    const AppConfig& config_;
    PositionManager& pos_mgr_;
    FillCallback fill_callback_;

    mutable std::mutex fills_mutex_;
    std::deque<Fill> recent_fills_;
    static constexpr size_t MAX_RECENT_FILLS = 500;

    std::atomic<uint64_t> trade_id_counter_{1};
    std::atomic<uint64_t> order_id_counter_{1};
    std::atomic<uint64_t> total_fills_{0};
    std::atomic<uint64_t> total_rejects_{0};
};

} // namespace efx
