#include "efx/pricing/volatility.hpp"
#include <cmath>
#include <algorithm>
#include <numeric>

namespace efx {

VolatilityEstimator::VolatilityEstimator(Params params) : params_(params) {}

void VolatilityEstimator::on_fair_value(const std::string& pair, double mid, Timestamp ts) {
    auto& state = states_[pair];

    if (state.last_mid > 0.0) {
        double log_return = std::log(mid / state.last_mid);

        auto dt = std::chrono::duration_cast<std::chrono::microseconds>(ts - state.last_ts);
        double dt_seconds = dt.count() / 1'000'000.0;
        if (dt_seconds < 1e-9) dt_seconds = 1e-6;

        // Scale return to per-second
        double scaled_return = log_return / std::sqrt(dt_seconds);

        state.returns.push_back(scaled_return);
        if (state.returns.size() > params_.window_ticks) {
            state.returns.pop_front();
        }

        // Compute realized volatility (standard deviation of returns)
        if (state.returns.size() >= 10) {
            double mean = std::accumulate(state.returns.begin(), state.returns.end(), 0.0)
                          / static_cast<double>(state.returns.size());
            double sum_sq = 0.0;
            for (double r : state.returns) {
                double d = r - mean;
                sum_sq += d * d;
            }
            double variance = sum_sq / static_cast<double>(state.returns.size() - 1);
            state.current_vol = std::sqrt(variance) * params_.annualization_factor;
        }
    }

    state.last_mid = mid;
    state.last_ts = ts;
}

double VolatilityEstimator::get_volatility(const std::string& pair) const {
    auto it = states_.find(pair);
    return (it != states_.end()) ? it->second.current_vol : 0.0;
}

VolatilityEstimator::Regime
VolatilityEstimator::get_regime(const std::string& pair, double baseline) const {
    double vol = get_volatility(pair);
    if (baseline <= 0.0) return Regime::Normal;

    double ratio = vol / baseline;
    if (ratio < 0.7) return Regime::Low;
    if (ratio < 1.5) return Regime::Normal;
    if (ratio < 3.0) return Regime::Elevated;
    return Regime::Crisis;
}

} // namespace efx
