#pragma once

#include "efx/core/types.hpp"
#include "efx/core/config.hpp"
#include "efx/market_data/venue_book.hpp"

#include <unordered_map>
#include <string>

namespace efx {

// Computes fair-value mid-price using VWAP aggregation + power-smoothed EWMA.
class FairValueEngine {
public:
    explicit FairValueEngine(const AppConfig& config);

    // Update fair value for all pairs based on current venue book state.
    void update(const VenueBookManager& book, Timestamp now);

    // Get the latest fair value for a pair.
    const FairValue* get(const std::string& pair) const;

    // Get all fair values.
    const std::unordered_map<std::string, FairValue>& all() const { return fair_values_; }

    // Tuning parameters
    void set_alpha(double a) { alpha_ = a; }
    void set_power(double p) { power_ = p; }

private:
    double compute_confidence(const VenueBookManager::AggregatedQuote& agg,
                              const std::string& pair,
                              const VenueBookManager& book) const;

    const AppConfig& config_;

    double alpha_ = 0.15;  // EWMA smoothing factor
    double power_ = 0.85;  // outlier compression exponent

    std::unordered_map<std::string, FairValue> fair_values_;
    std::unordered_map<std::string, double> prev_mids_;
};

} // namespace efx
