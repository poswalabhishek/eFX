#pragma once

#include "efx/core/types.hpp"
#include "efx/order/order_manager.hpp"
#include "efx/pricing/client_pricer.hpp"
#include "efx/pricing/fair_value.hpp"

#include <random>
#include <string>
#include <unordered_map>

namespace efx {

// Simulates client trade flow based on configurable archetypes.
// Toxic clients trade with momentum (informed), benign clients trade randomly.
class ClientSimulator {
public:
    ClientSimulator(OrderManager& oms, const ClientPricer& pricer,
                    uint64_t seed = 123);

    // Called each engine step. Generates orders based on client arrival rates.
    void step(const FairValueEngine& fv_engine, double true_mid_eurusd,
              double time_step_ms);

    uint64_t total_orders() const { return total_orders_; }

private:
    struct ClientProfile {
        double arrival_rate_per_sec;  // Poisson rate
        double toxicity;              // [0,1] how informed (0 = random, 1 = perfect foresight)
        double avg_size;
        std::vector<std::string> preferred_pairs;
        std::vector<std::string> preferred_venues;
    };

    void init_profiles();
    void maybe_generate_order(const std::string& client_id,
                              const ClientProfile& profile,
                              const FairValueEngine& fv_engine,
                              double time_step_ms);

    OrderManager& oms_;
    const ClientPricer& pricer_;
    std::mt19937_64 rng_;

    std::unordered_map<std::string, ClientProfile> profiles_;
    uint64_t total_orders_ = 0;

    // Track last true mid for directional bias
    double prev_true_mid_ = 0.0;
};

} // namespace efx
