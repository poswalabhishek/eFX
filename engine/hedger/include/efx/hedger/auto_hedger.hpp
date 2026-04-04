#pragma once

#include "efx/core/types.hpp"
#include "efx/core/config.hpp"
#include "efx/order/order_manager.hpp"
#include "efx/market_data/venue_book.hpp"

#include <functional>
#include <deque>
#include <mutex>
#include <atomic>

namespace efx {

// Immediately hedges client fills on the best available venue.
// Selects the venue with the tightest price on the side we need,
// adds simulated slippage, and records the hedge execution.
class AutoHedger {
public:
    using HedgeCallback = std::function<void(const HedgeExecution&)>;

    explicit AutoHedger(const AppConfig& config);

    void set_hedge_callback(HedgeCallback cb) { callback_ = std::move(cb); }

    // Called when a client fill occurs. Hedges the opposite side on the interbank.
    void on_client_fill(const Fill& fill, const VenueBookManager& book);

    std::vector<HedgeExecution> recent_hedges(size_t limit = 50) const;

    uint64_t total_hedges() const { return total_hedges_.load(); }
    double total_slippage_usd() const;

    struct HedgeStats {
        uint64_t total_hedges;
        double total_slippage_usd;
        double avg_latency_us;
        double avg_slippage_bps;
    };
    HedgeStats stats() const;

private:
    const AppConfig& config_;
    HedgeCallback callback_;

    mutable std::mutex mutex_;
    std::deque<HedgeExecution> recent_;
    static constexpr size_t MAX_RECENT = 500;

    std::atomic<uint64_t> total_hedges_{0};
    std::atomic<uint64_t> hedge_id_counter_{1};
    double cumulative_slippage_usd_ = 0.0;
    double cumulative_latency_us_ = 0.0;
    double cumulative_slippage_bps_ = 0.0;
};

// Tracks decomposed PnL from actual fills and hedges.
class PnlEngine {
public:
    struct PnlBreakdown {
        double spread_capture = 0.0;    // half-spread earned on client fills
        double hedge_cost = 0.0;        // slippage lost on hedge executions
        double realized_pnl = 0.0;      // from closed positions
        double unrealized_pnl = 0.0;    // mark-to-market of open positions

        double net_pnl() const { return spread_capture + hedge_cost + realized_pnl + unrealized_pnl; }
    };

    void on_client_fill(const Fill& fill);
    void on_hedge(const HedgeExecution& hedge);
    void set_position_pnl(double realized, double unrealized);

    PnlBreakdown breakdown() const;

    // Per-client PnL
    double client_pnl(const std::string& client_id) const;
    std::unordered_map<std::string, double> all_client_pnl() const;

    // Per-pair PnL
    double pair_pnl(const std::string& pair) const;

    // Per-venue PnL
    double venue_pnl(const std::string& venue) const;

private:
    mutable std::mutex mutex_;
    PnlBreakdown pnl_;
    std::unordered_map<std::string, double> client_pnl_;
    std::unordered_map<std::string, double> pair_pnl_;
    std::unordered_map<std::string, double> venue_pnl_;
};

} // namespace efx
