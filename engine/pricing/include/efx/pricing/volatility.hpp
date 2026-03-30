#pragma once

#include "efx/core/types.hpp"

#include <deque>
#include <unordered_map>
#include <string>

namespace efx {

// Real-time volatility estimator using tick-level returns.
// Computes annualized volatility per pair over a rolling window.
class VolatilityEstimator {
public:
    struct Params {
        size_t window_ticks = 500;
        double annualization_factor = std::sqrt(252.0 * 24.0 * 3600.0); // from per-second to annual
    };

    VolatilityEstimator() : VolatilityEstimator(Params{}) {}
    explicit VolatilityEstimator(Params params);

    void on_fair_value(const std::string& pair, double mid, Timestamp ts);

    double get_volatility(const std::string& pair) const;

    // Regime detection: is current vol elevated relative to baseline?
    enum class Regime { Low, Normal, Elevated, Crisis };
    Regime get_regime(const std::string& pair, double baseline) const;

private:
    struct PairVolState {
        std::deque<double> returns;
        double last_mid = 0.0;
        Timestamp last_ts{};
        double current_vol = 0.0;
    };

    Params params_;
    std::unordered_map<std::string, PairVolState> states_;
};

} // namespace efx
