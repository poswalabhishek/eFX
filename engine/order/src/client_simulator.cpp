#include "efx/order/client_simulator.hpp"
#include <spdlog/spdlog.h>
#include <cmath>
#include <algorithm>

namespace efx {

ClientSimulator::ClientSimulator(OrderManager& oms, const ClientPricer& pricer,
                                 uint64_t seed)
    : oms_(oms), pricer_(pricer), rng_(seed) {
    init_profiles();
    spdlog::info("ClientSimulator: {} profiles loaded", profiles_.size());
}

void ClientSimulator::init_profiles() {
    profiles_["toxic_hft_firm"] = {
        .arrival_rate_per_sec = 2.0,
        .toxicity = 0.8,
        .avg_size = 2'000'000,
        .preferred_pairs = {"EURUSD", "USDJPY"},
        .preferred_venues = {"EBS", "Reuters"},
    };
    profiles_["algo_firm_momentum"] = {
        .arrival_rate_per_sec = 1.0,
        .toxicity = 0.6,
        .avg_size = 5'000'000,
        .preferred_pairs = {"EURUSD", "GBPUSD", "USDJPY", "AUDUSD"},
        .preferred_venues = {"EBS", "Hotspot"},
    };
    profiles_["hedge_fund_alpha"] = {
        .arrival_rate_per_sec = 0.5,
        .toxicity = 0.4,
        .avg_size = 10'000'000,
        .preferred_pairs = {"EURUSD", "GBPUSD", "USDJPY"},
        .preferred_venues = {"EBS", "Reuters", "Currenex"},
    };
    profiles_["retail_aggregator"] = {
        .arrival_rate_per_sec = 3.0,
        .toxicity = 0.05,
        .avg_size = 500'000,
        .preferred_pairs = {"EURUSD", "GBPUSD", "USDJPY", "AUDUSD", "USDCAD"},
        .preferred_venues = {"Hotspot", "SDP"},
    };
    profiles_["real_money_pension"] = {
        .arrival_rate_per_sec = 0.1,
        .toxicity = 0.0,
        .avg_size = 25'000'000,
        .preferred_pairs = {"EURUSD", "GBPUSD", "USDJPY"},
        .preferred_venues = {"Reuters", "Currenex", "SDP"},
    };
    profiles_["corp_treasury_acme"] = {
        .arrival_rate_per_sec = 0.05,
        .toxicity = 0.0,
        .avg_size = 15'000'000,
        .preferred_pairs = {"EURUSD", "GBPUSD"},
        .preferred_venues = {"SDP", "Currenex"},
    };
    profiles_["sovereign_fund"] = {
        .arrival_rate_per_sec = 0.02,
        .toxicity = 0.0,
        .avg_size = 50'000'000,
        .preferred_pairs = {"EURUSD", "USDJPY"},
        .preferred_venues = {"EBS", "Reuters"},
    };
    profiles_["bank_flow_dealer"] = {
        .arrival_rate_per_sec = 0.3,
        .toxicity = 0.1,
        .avg_size = 8'000'000,
        .preferred_pairs = {"EURUSD", "GBPUSD", "USDJPY", "USDCHF"},
        .preferred_venues = {"EBS", "Reuters"},
    };
}

void ClientSimulator::step(const FairValueEngine& fv_engine,
                           double true_mid_eurusd, double time_step_ms) {
    for (const auto& [client_id, profile] : profiles_) {
        maybe_generate_order(client_id, profile, fv_engine, time_step_ms);
    }
    prev_true_mid_ = true_mid_eurusd;
}

void ClientSimulator::maybe_generate_order(
    const std::string& client_id,
    const ClientProfile& profile,
    const FairValueEngine& fv_engine,
    double time_step_ms) {

    // Poisson arrival: probability of order in this time step
    double prob = profile.arrival_rate_per_sec * time_step_ms / 1000.0;
    std::uniform_real_distribution<double> uniform(0.0, 1.0);
    if (uniform(rng_) > prob) return;

    // Pick a random pair from preferences
    if (profile.preferred_pairs.empty()) return;
    std::uniform_int_distribution<size_t> pair_dist(0, profile.preferred_pairs.size() - 1);
    const auto& pair = profile.preferred_pairs[pair_dist(rng_)];

    auto fv = fv_engine.get(pair);
    if (!fv || fv->mid <= 0.0) return;

    // Determine direction
    Side side;
    if (profile.toxicity > 0.1 && prev_true_mid_ > 0.0) {
        // Toxic clients trade with momentum
        double momentum = fv->mid - prev_true_mid_;
        double bias = std::tanh(momentum * 100000.0 * profile.toxicity);
        side = (uniform(rng_) < 0.5 + bias * 0.4) ? Side::Buy : Side::Sell;
    } else {
        side = (uniform(rng_) < 0.5) ? Side::Buy : Side::Sell;
    }

    // Size with some randomness
    std::normal_distribution<double> size_dist(profile.avg_size, profile.avg_size * 0.3);
    double amount = std::max(100'000.0, size_dist(rng_));
    amount = std::round(amount / 100'000.0) * 100'000.0; // round to 100K

    // Get the client's quoted price
    auto client_cfg = pricer_.get_client(client_id);
    if (!client_cfg) return;

    auto pair_it = std::find_if(
        pricer_.get_all_clients().begin(),
        pricer_.get_all_clients().end(),
        [&](const auto& c) { return c.client_id == client_id; }
    );

    // Use fair value mid with a small offset as the fill price
    auto pair_cfg_it = fv_engine.all().find(pair);
    if (pair_cfg_it == fv_engine.all().end()) return;

    Price fill_price = (side == Side::Buy) ? fv->mid + 0.00001 : fv->mid - 0.00001;

    // Pick venue
    std::uniform_int_distribution<size_t> venue_dist(0, profile.preferred_venues.size() - 1);
    const auto& venue = profile.preferred_venues[venue_dist(rng_)];

    Order order{
        .order_id = oms_.next_order_id(),
        .client_id = ClientId{client_id},
        .pair = pair,
        .side = side,
        .amount = amount,
        .limit_price = fill_price,
        .quote_id = 0,
        .venue = VenueId{venue},
        .created_at = Timestamp::clock::now(),
        .wall_time = WallClock::clock::now(),
    };

    oms_.submit_order(order, fv->mid);
    total_orders_++;
}

} // namespace efx
