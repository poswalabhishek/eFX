#include "efx/market_data/venue_book.hpp"
#include <spdlog/spdlog.h>
#include <cmath>
#include <algorithm>

namespace efx {

VenueBookManager::VenueBookManager(const AppConfig& config) : config_(config) {
    for (const auto& pair : config_.pairs) {
        books_[pair.symbol] = {};
    }
}

void VenueBookManager::on_tick(const VenueTick& tick) {
    auto& pair_book = books_[tick.pair];
    pair_book[tick.venue.id] = VenueQuote{
        .bid = tick.bid,
        .ask = tick.ask,
        .bid_size = tick.bid_size,
        .ask_size = tick.ask_size,
        .last_update = tick.timestamp,
        .is_stale = false,
    };
}

void VenueBookManager::check_staleness(Timestamp now) {
    for (auto& [pair, venues] : books_) {
        for (auto& [venue_id, quote] : venues) {
            auto it = config_.venue_map.find(venue_id);
            if (it == config_.venue_map.end()) continue;

            double threshold_ms = it->second->staleness_threshold_ms;
            auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - quote.last_update);

            quote.is_stale = (age.count() > threshold_ms);
        }
    }
}

double VenueBookManager::compute_weight(
    const std::string& venue_id, const VenueQuote& quote, Timestamp now) const {

    if (quote.is_stale) return 0.0;

    auto it = config_.venue_map.find(venue_id);
    if (it == config_.venue_map.end()) return 0.0;

    const auto& venue = *it->second;
    double base_weight = (venue.venue_type == "primary") ? 2.0 : 1.0;

    // Freshness decay: exponential decay based on age
    auto age_ms = std::chrono::duration_cast<std::chrono::microseconds>(
        now - quote.last_update).count() / 1000.0;
    double freshness = std::exp(-age_ms / venue.staleness_threshold_ms);

    // Volume weight (use bid+ask size as proxy)
    double vol_weight = std::log1p((quote.bid_size + quote.ask_size) / 1'000'000.0);

    return base_weight * freshness * vol_weight * venue.reliability;
}

std::optional<VenueBookManager::AggregatedQuote>
VenueBookManager::get_aggregated(const std::string& pair) const {
    auto it = books_.find(pair);
    if (it == books_.end() || it->second.empty()) return std::nullopt;

    auto now = Timestamp::clock::now();
    double weighted_sum = 0.0;
    double total_weight = 0.0;
    int active_count = 0;
    Price best_bid = 0.0;
    Price best_ask = std::numeric_limits<double>::max();

    for (const auto& [venue_id, quote] : it->second) {
        double w = compute_weight(venue_id, quote, now);
        if (w <= 0.0) continue;

        double mid = (quote.bid + quote.ask) / 2.0;
        weighted_sum += w * mid;
        total_weight += w;
        active_count++;

        best_bid = std::max(best_bid, quote.bid);
        best_ask = std::min(best_ask, quote.ask);
    }

    if (total_weight <= 0.0 || active_count == 0) return std::nullopt;

    return AggregatedQuote{
        .weighted_mid = weighted_sum / total_weight,
        .total_weight = total_weight,
        .active_venue_count = active_count,
        .best_bid = best_bid,
        .best_ask = best_ask,
    };
}

const VenueQuote* VenueBookManager::get_venue_quote(
    const std::string& pair, const std::string& venue) const {
    auto pit = books_.find(pair);
    if (pit == books_.end()) return nullptr;
    auto vit = pit->second.find(venue);
    return (vit != pit->second.end()) ? &vit->second : nullptr;
}

const std::unordered_map<std::string, VenueQuote>*
VenueBookManager::get_pair_quotes(const std::string& pair) const {
    auto it = books_.find(pair);
    return (it != books_.end()) ? &it->second : nullptr;
}

} // namespace efx
