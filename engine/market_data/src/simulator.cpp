#include "efx/market_data/simulator.hpp"
#include <spdlog/spdlog.h>
#include <cmath>
#include <algorithm>

namespace efx {

const std::unordered_map<std::string, double> MarketDataSimulator::REFERENCE_PRICES = {
    {"EURUSD", 1.08500}, {"USDJPY", 149.500}, {"GBPUSD", 1.27300},
    {"AUDUSD", 0.65200}, {"USDCHF", 0.88350}, {"USDCAD", 1.36200},
    {"NZDUSD", 0.59800}, {"EURGBP", 0.85250}, {"EURJPY", 162.100},
    {"GBPJPY", 190.250},
};

MarketDataSimulator::MarketDataSimulator(const AppConfig& config, uint64_t seed)
    : config_(config), rng_(seed),
      sim_time_(Timestamp::clock::now()),
      sim_wall_time_(WallClock::clock::now()) {
    init_pair_states();
    init_venue_states();
    spdlog::info("MarketDataSimulator initialized with {} pairs, {} venues",
                 pair_states_.size(), venue_states_.size());
}

void MarketDataSimulator::init_pair_states() {
    for (const auto& pair : config_.pairs) {
        auto it = REFERENCE_PRICES.find(pair.symbol);
        double ref = (it != REFERENCE_PRICES.end()) ? it->second : 1.0;

        double vol = 0.065; // ~6.5% annualized default
        if (pair.symbol.find("JPY") != std::string::npos) vol = 0.080;
        if (pair.symbol == "GBPJPY") vol = 0.095;

        pair_states_[pair.symbol] = PairState{
            .true_mid = ref,
            .drift = 0.0,
            .volatility = vol,
        };
    }
}

void MarketDataSimulator::init_venue_states() {
    for (const auto& venue : config_.venues) {
        double tick_prob = (venue.avg_tick_rate_hz * time_step_ms_) / 1000.0;
        tick_prob = std::clamp(tick_prob, 0.0, 0.95);

        double stale_prob = 1.0 - venue.reliability;

        venue_states_[venue.id] = VenueState{
            .latency_offset_ms = venue.latency_offset_ms,
            .spread_half_pips = venue.base_spread_pips / 2.0,
            .staleness_prob = stale_prob * time_step_ms_ / 1000.0,
            .tick_prob_per_step = tick_prob,
        };
    }
}

void MarketDataSimulator::evolve_true_prices() {
    // Jump-diffusion: dS = μ·dt + σ·dW + J·dN
    double dt = time_step_ms_ / (365.25 * 24.0 * 3600.0 * 1000.0); // in years

    for (auto& [pair, state] : pair_states_) {
        double dW = normal_(rng_) * std::sqrt(dt);
        double diffusion = state.volatility * state.true_mid * dW;

        // Poisson jump (~3 per day on average)
        double jump = 0.0;
        double jump_rate = 3.0 / (24.0 * 3600.0 * 1000.0) * time_step_ms_;
        if (uniform_(rng_) < jump_rate) {
            double jump_size = normal_(rng_) * state.volatility * state.true_mid * 0.001;
            jump = jump_size;
        }

        state.true_mid += diffusion + jump;
        state.true_mid = std::max(state.true_mid, 0.00001);
    }
}

VenueTick MarketDataSimulator::generate_venue_tick(
    const std::string& pair, const VenueConfig& venue) {

    const auto& ps = pair_states_.at(pair);
    const auto& vs = venue_states_.at(venue.id);

    auto pair_it = config_.pair_map.find(pair);
    double pip = (pair_it != config_.pair_map.end()) ? pair_it->second->pip_size : 0.00001;

    double noise = normal_(rng_) * pip * 0.5;
    double observed_mid = ps.true_mid + noise;

    double half_spread = vs.spread_half_pips * pip;
    // Add small random spread variation
    half_spread *= (1.0 + normal_(rng_) * 0.1);
    half_spread = std::max(half_spread, pip * 0.05);

    double size_base = 1'000'000.0; // 1M base
    double bid_size = size_base * (1.0 + uniform_(rng_) * 4.0);
    double ask_size = size_base * (1.0 + uniform_(rng_) * 4.0);

    return VenueTick{
        .venue = VenueId{venue.id},
        .pair = pair,
        .bid = observed_mid - half_spread,
        .ask = observed_mid + half_spread,
        .bid_size = bid_size,
        .ask_size = ask_size,
        .timestamp = sim_time_,
        .wall_time = sim_wall_time_,
    };
}

void MarketDataSimulator::step(TickCallback on_tick) {
    evolve_true_prices();

    // Keep quote timestamps aligned with wall/steady clocks used by staleness logic.
    // Advancing by a fixed step can drift under load and freeze fair-value updates.
    sim_time_ = Timestamp::clock::now();
    sim_wall_time_ = WallClock::clock::now();

    for (const auto& venue : config_.venues) {
        const auto& vs = venue_states_.at(venue.id);

        for (const auto& pair_cfg : config_.pairs) {
            // Each venue ticks with a probability based on its tick rate
            if (uniform_(rng_) > vs.tick_prob_per_step) continue;

            // Occasional staleness
            if (uniform_(rng_) < vs.staleness_prob) continue;

            auto tick = generate_venue_tick(pair_cfg.symbol, venue);
            on_tick(tick);
        }
    }
}

double MarketDataSimulator::true_mid(const std::string& pair) const {
    auto it = pair_states_.find(pair);
    return (it != pair_states_.end()) ? it->second.true_mid : 0.0;
}

} // namespace efx
