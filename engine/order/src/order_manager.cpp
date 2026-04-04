#include "efx/order/order_manager.hpp"
#include <spdlog/spdlog.h>
#include <cmath>

namespace efx {

// ---- PositionManager ----

void PositionManager::on_fill(const Fill& fill) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& pos = positions_[fill.pair];
    pos.pair = fill.pair;

    double signed_amount = (fill.side == Side::Buy) ? fill.amount : -fill.amount;

    // If position is being reduced or flattened, realize PnL
    if ((pos.position > 0 && signed_amount < 0) ||
        (pos.position < 0 && signed_amount > 0)) {
        double close_amount = std::min(std::abs(signed_amount), std::abs(pos.position));
        double direction = (pos.position > 0) ? 1.0 : -1.0;
        double pnl = direction * close_amount * (fill.price - pos.avg_entry_price);
        pos.realized_pnl += pnl;
    }

    // Update position
    double old_pos = pos.position;
    pos.position += signed_amount;

    // Update average entry price
    if (std::abs(pos.position) < 0.01) {
        pos.avg_entry_price = 0.0;
        pos.position = 0.0;
    } else if ((old_pos >= 0 && signed_amount > 0) || (old_pos <= 0 && signed_amount < 0)) {
        // Adding to position: weighted average
        double total = std::abs(old_pos) + std::abs(signed_amount);
        pos.avg_entry_price = (std::abs(old_pos) * pos.avg_entry_price +
                               std::abs(signed_amount) * fill.price) / total;
    }
    // If flipping direction, new entry is the fill price
    if ((old_pos > 0 && pos.position < 0) || (old_pos < 0 && pos.position > 0)) {
        pos.avg_entry_price = fill.price;
    }

    pos.trade_count++;
}

void PositionManager::mark_to_market(const std::string& pair, double mid) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = positions_.find(pair);
    if (it == positions_.end()) return;

    auto& pos = it->second;
    pos.last_mid = mid;
    if (std::abs(pos.position) > 0.01 && pos.avg_entry_price > 0.0) {
        pos.unrealized_pnl = pos.position * (mid - pos.avg_entry_price);
    } else {
        pos.unrealized_pnl = 0.0;
    }
}

const PositionManager::PairPosition* PositionManager::get_position(const std::string& pair) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = positions_.find(pair);
    return (it != positions_.end()) ? &it->second : nullptr;
}

std::vector<PositionManager::PairPosition> PositionManager::get_all_positions() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<PairPosition> result;
    result.reserve(positions_.size());
    for (const auto& [_, p] : positions_) {
        result.push_back(p);
    }
    return result;
}

double PositionManager::total_realized_pnl() const {
    std::lock_guard<std::mutex> lock(mutex_);
    double total = 0.0;
    for (const auto& [_, p] : positions_) total += p.realized_pnl;
    return total;
}

double PositionManager::total_unrealized_pnl() const {
    std::lock_guard<std::mutex> lock(mutex_);
    double total = 0.0;
    for (const auto& [_, p] : positions_) total += p.unrealized_pnl;
    return total;
}

double PositionManager::net_position_usd(const std::string& pair) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = positions_.find(pair);
    return (it != positions_.end()) ? it->second.position : 0.0;
}

// ---- OrderManager ----

OrderManager::OrderManager(const AppConfig& config, PositionManager& pos_mgr)
    : config_(config), pos_mgr_(pos_mgr) {}

bool OrderManager::validate_order(const Order& order, std::string& reason) const {
    if (order.amount <= 0) {
        reason = "Invalid amount";
        return false;
    }
    if (order.pair.empty()) {
        reason = "Missing pair";
        return false;
    }
    if (config_.pair_map.find(order.pair) == config_.pair_map.end()) {
        reason = "Unknown pair: " + order.pair;
        return false;
    }
    // Position limit check
    double current_pos = pos_mgr_.net_position_usd(order.pair);
    double new_pos = current_pos + ((order.side == Side::Buy) ? order.amount : -order.amount);
    if (std::abs(new_pos) > config_.risk.max_per_pair_usd) {
        reason = "Position limit exceeded";
        return false;
    }
    return true;
}

FillResult OrderManager::submit_order(const Order& order, Price current_mid) {
    std::string reject_reason;
    if (!validate_order(order, reject_reason)) {
        total_rejects_.fetch_add(1);
        return FillResult{
            .status = FillStatus::Rejected,
            .fill = {},
            .reject_reason = reject_reason,
        };
    }

    // Execute at the quoted price (market order semantics)
    Fill fill{
        .trade_id = next_trade_id(),
        .quote_id = order.quote_id,
        .client_id = order.client_id,
        .pair = order.pair,
        .side = order.side,
        .price = order.limit_price,
        .amount = order.amount,
        .venue = order.venue,
        .mid_at_fill = current_mid,
        .spread_bps = std::abs(order.limit_price - current_mid) / current_mid * 10000.0,
        .session = "london",
        .timestamp = Timestamp::clock::now(),
        .wall_time = WallClock::clock::now(),
    };

    // Update position
    pos_mgr_.on_fill(fill);

    // Store in recent fills
    {
        std::lock_guard<std::mutex> lock(fills_mutex_);
        recent_fills_.push_back(fill);
        if (recent_fills_.size() > MAX_RECENT_FILLS) {
            recent_fills_.pop_front();
        }
    }

    total_fills_.fetch_add(1);

    // Notify callback
    if (fill_callback_) {
        fill_callback_(fill);
    }

    return FillResult{
        .status = FillStatus::Filled,
        .fill = fill,
        .reject_reason = "",
    };
}

std::vector<Fill> OrderManager::recent_fills(size_t limit) const {
    std::lock_guard<std::mutex> lock(fills_mutex_);
    size_t count = std::min(limit, recent_fills_.size());
    return {recent_fills_.end() - count, recent_fills_.end()};
}

} // namespace efx
