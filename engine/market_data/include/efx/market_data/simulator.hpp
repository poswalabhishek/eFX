#pragma once

#include "efx/core/types.hpp"
#include "efx/core/config.hpp"
#include "efx/market_data/venue_book.hpp"

#include <random>
#include <unordered_map>
#include <vector>
#include <functional>

namespace efx {

// Generates realistic FX market data using a jump-diffusion process
// for a shared "true" mid-price, with per-venue noise and latency.
class MarketDataSimulator {
public:
    using TickCallback = std::function<void(const VenueTick&)>;

    explicit MarketDataSimulator(const AppConfig& config, uint64_t seed = 42);

    // Advance simulation by one step (generates ticks for all venues/pairs).
    // Calls the callback for each tick produced.
    void step(TickCallback on_tick);

    // Get the true mid-price for a pair (for alpha simulation).
    double true_mid(const std::string& pair) const;

    void set_time_step_ms(double ms) { time_step_ms_ = ms; }

private:
    struct PairState {
        double true_mid;
        double drift;
        double volatility;
    };

    struct VenueState {
        double latency_offset_ms;
        double spread_half_pips;
        double staleness_prob;
        double tick_prob_per_step;
    };

    void init_pair_states();
    void init_venue_states();
    void evolve_true_prices();
    VenueTick generate_venue_tick(const std::string& pair, const VenueConfig& venue);

    const AppConfig& config_;
    double time_step_ms_ = 1.0; // 1ms default step

    std::mt19937_64 rng_;
    std::normal_distribution<double> normal_{0.0, 1.0};
    std::uniform_real_distribution<double> uniform_{0.0, 1.0};

    std::unordered_map<std::string, PairState> pair_states_;
    std::unordered_map<std::string, VenueState> venue_states_;

    Timestamp sim_time_;
    WallClock sim_wall_time_;

    // Initial reference prices for each pair
    static const std::unordered_map<std::string, double> REFERENCE_PRICES;
};

} // namespace efx
