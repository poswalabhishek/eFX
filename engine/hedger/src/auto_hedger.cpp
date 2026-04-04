#include "efx/hedger/auto_hedger.hpp"
#include <spdlog/spdlog.h>
#include <cmath>
#include <algorithm>
#include <random>

namespace efx {

// ---- AutoHedger ----

AutoHedger::AutoHedger(const AppConfig& config) : config_(config) {}

void AutoHedger::on_client_fill(const Fill& fill, const VenueBookManager& book) {
    auto hedge_start = Timestamp::clock::now();

    // We need to hedge the opposite side of the client's trade.
    // Client bought -> we sold to them -> we are short -> hedge by buying on interbank.
    Side hedge_side = opposite(fill.side);
    auto pair_quotes = book.get_pair_quotes(fill.pair);
    if (!pair_quotes) return;

    // Find best venue for our hedge side
    Price best_price = 0.0;
    std::string best_venue;

    for (const auto& [venue_id, quote] : *pair_quotes) {
        if (quote.is_stale) continue;

        Price venue_price = (hedge_side == Side::Buy) ? quote.ask : quote.bid;
        if (venue_price <= 0.0) continue;

        bool is_better = false;
        if (hedge_side == Side::Buy) {
            is_better = (best_price == 0.0 || venue_price < best_price);
        } else {
            is_better = (best_price == 0.0 || venue_price > best_price);
        }

        if (is_better) {
            best_price = venue_price;
            best_venue = venue_id;
        }
    }

    if (best_price <= 0.0 || best_venue.empty()) return;

    // Simulate small slippage (0-0.3 pips random)
    thread_local std::mt19937_64 rng(std::random_device{}());
    auto pair_it = config_.pair_map.find(fill.pair);
    double pip = (pair_it != config_.pair_map.end()) ? pair_it->second->pip_size : 0.00001;
    std::uniform_real_distribution<double> slip_dist(0.0, 0.3);
    double slippage_pips = slip_dist(rng);
    double slippage_price = slippage_pips * pip;

    Price hedge_price = (hedge_side == Side::Buy)
        ? best_price + slippage_price
        : best_price - slippage_price;

    auto hedge_end = Timestamp::clock::now();
    auto latency = std::chrono::duration_cast<std::chrono::microseconds>(hedge_end - hedge_start);

    double slippage_bps = std::abs(hedge_price - fill.mid_at_fill) / fill.mid_at_fill * 10000.0;

    HedgeExecution hedge{
        .hedge_id = hedge_id_counter_.fetch_add(1),
        .triggering_trade_id = fill.trade_id,
        .pair = fill.pair,
        .side = hedge_side,
        .price = hedge_price,
        .amount = fill.amount,
        .venue = VenueId{best_venue},
        .slippage_bps = slippage_bps,
        .latency_us = static_cast<uint64_t>(latency.count()),
        .timestamp = hedge_end,
        .wall_time = WallClock::clock::now(),
    };

    {
        std::lock_guard<std::mutex> lock(mutex_);
        recent_.push_back(hedge);
        if (recent_.size() > MAX_RECENT) recent_.pop_front();
        cumulative_slippage_usd_ += slippage_bps * fill.amount * 0.0001;
        cumulative_latency_us_ += latency.count();
        cumulative_slippage_bps_ += slippage_bps;
    }

    total_hedges_.fetch_add(1);

    if (callback_) callback_(hedge);
}

std::vector<HedgeExecution> AutoHedger::recent_hedges(size_t limit) const {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t count = std::min(limit, recent_.size());
    return {recent_.end() - count, recent_.end()};
}

double AutoHedger::total_slippage_usd() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cumulative_slippage_usd_;
}

AutoHedger::HedgeStats AutoHedger::stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t n = total_hedges_.load();
    return {
        .total_hedges = n,
        .total_slippage_usd = cumulative_slippage_usd_,
        .avg_latency_us = (n > 0) ? cumulative_latency_us_ / n : 0.0,
        .avg_slippage_bps = (n > 0) ? cumulative_slippage_bps_ / n : 0.0,
    };
}

// ---- PnlEngine ----

void PnlEngine::on_client_fill(const Fill& fill) {
    std::lock_guard<std::mutex> lock(mutex_);
    double half_spread = fill.spread_bps * fill.amount * 0.0001 * 0.5;
    pnl_.spread_capture += half_spread;

    client_pnl_[fill.client_id.id] += half_spread;
    pair_pnl_[fill.pair] += half_spread;
    venue_pnl_[fill.venue.id] += half_spread;
}

void PnlEngine::on_hedge(const HedgeExecution& hedge) {
    std::lock_guard<std::mutex> lock(mutex_);
    double cost = -(hedge.slippage_bps * hedge.amount * 0.0001);
    pnl_.hedge_cost += cost;
    venue_pnl_[hedge.venue.id] += cost;
    pair_pnl_[hedge.pair] += cost;
}

void PnlEngine::set_position_pnl(double realized, double unrealized) {
    std::lock_guard<std::mutex> lock(mutex_);
    pnl_.realized_pnl = realized;
    pnl_.unrealized_pnl = unrealized;
}

PnlEngine::PnlBreakdown PnlEngine::breakdown() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pnl_;
}

double PnlEngine::client_pnl(const std::string& client_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = client_pnl_.find(client_id);
    return (it != client_pnl_.end()) ? it->second : 0.0;
}

std::unordered_map<std::string, double> PnlEngine::all_client_pnl() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return client_pnl_;
}

double PnlEngine::pair_pnl(const std::string& pair) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = pair_pnl_.find(pair);
    return (it != pair_pnl_.end()) ? it->second : 0.0;
}

double PnlEngine::venue_pnl(const std::string& venue) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = venue_pnl_.find(venue);
    return (it != venue_pnl_.end()) ? it->second : 0.0;
}

} // namespace efx
