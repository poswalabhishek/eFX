#include "efx/pricing/fair_value.hpp"
#include <spdlog/spdlog.h>
#include <cmath>
#include <algorithm>

namespace efx {

FairValueEngine::FairValueEngine(const AppConfig& config) : config_(config) {
    for (const auto& pair : config_.pairs) {
        fair_values_[pair.symbol] = FairValue{
            .pair = pair.symbol,
            .mid = 0.0,
            .confidence = 0.0,
            .volatility = 0.0,
            .num_active_venues = 0,
            .timestamp = {},
            .wall_time = {},
        };
        prev_mids_[pair.symbol] = 0.0;
    }
}

void FairValueEngine::update(const VenueBookManager& book, Timestamp now) {
    for (const auto& pair_cfg : config_.pairs) {
        const auto& sym = pair_cfg.symbol;
        auto agg = book.get_aggregated(sym);
        if (!agg.has_value()) continue;

        double raw_mid = agg->weighted_mid;
        auto& fv = fair_values_[sym];
        auto& prev_mid = prev_mids_[sym];

        if (prev_mid <= 0.0) {
            // First tick: initialize directly
            fv.mid = raw_mid;
            prev_mid = raw_mid;
        } else {
            // Power-smoothed EWMA on the delta
            double delta = raw_mid - fv.mid;
            double sign = (delta >= 0.0) ? 1.0 : -1.0;
            double smoothed_delta = sign * std::pow(std::abs(delta), power_);
            fv.mid = fv.mid + alpha_ * smoothed_delta;
            prev_mid = raw_mid;
        }

        fv.confidence = compute_confidence(*agg, sym, book);
        fv.num_active_venues = agg->active_venue_count;
        fv.timestamp = now;
        fv.wall_time = WallClock::clock::now();
    }
}

double FairValueEngine::compute_confidence(
    const VenueBookManager::AggregatedQuote& agg,
    const std::string& pair,
    const VenueBookManager& book) const {

    // Base confidence from number of active venues (0-4 venues -> 0.25-1.0)
    double venue_score = std::min(1.0, agg.active_venue_count / 4.0);

    // Dispersion: how much do venues disagree?
    double dispersion_score = 1.0;
    auto quotes = book.get_pair_quotes(pair);
    if (quotes && quotes->size() > 1) {
        double sum_sq_diff = 0.0;
        int count = 0;
        for (const auto& [vid, q] : *quotes) {
            if (q.is_stale) continue;
            double mid = (q.bid + q.ask) / 2.0;
            double diff = mid - agg.weighted_mid;
            sum_sq_diff += diff * diff;
            count++;
        }
        if (count > 1) {
            double rmsd = std::sqrt(sum_sq_diff / count);
            auto pair_it = config_.pair_map.find(pair);
            double pip = (pair_it != config_.pair_map.end()) ? pair_it->second->pip_size : 0.00001;
            double rmsd_pips = rmsd / pip;
            dispersion_score = std::max(0.0, 1.0 - rmsd_pips / 5.0);
        }
    }

    return std::clamp(venue_score * 0.5 + dispersion_score * 0.5, 0.0, 1.0);
}

const FairValue* FairValueEngine::get(const std::string& pair) const {
    auto it = fair_values_.find(pair);
    return (it != fair_values_.end()) ? &it->second : nullptr;
}

} // namespace efx
