#pragma once

#include "efx/core/types.hpp"
#include "efx/core/config.hpp"
#include "efx/pricing/spread_constructor.hpp"
#include "efx/pricing/volatility.hpp"

#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <atomic>

namespace efx {

// Per-client configuration used at pricing time.
struct ClientPricingConfig {
    std::string client_id;
    std::string name;
    TierId default_tier = TierId::Silver;
    double alpha_score = 0.0;
    bool is_active = true;

    // Per-pair overrides: pair -> (tier, adjustment_pct)
    struct PairOverride {
        TierId tier;
        double adjustment_pct;
    };
    std::unordered_map<std::string, PairOverride> pair_overrides;

    TierId tier_for_pair(const std::string& pair) const {
        auto it = pair_overrides.find(pair);
        return (it != pair_overrides.end()) ? it->second.tier : default_tier;
    }

    double override_pct_for_pair(const std::string& pair) const {
        auto it = pair_overrides.find(pair);
        return (it != pair_overrides.end()) ? it->second.adjustment_pct : 0.0;
    }
};

// Manages client configs and generates per-client prices from fair values.
// Thread-safe: config updates can happen from a reload thread while
// the pricing thread reads.
class ClientPricer {
public:
    explicit ClientPricer(const AppConfig& config);

    // Replace all client configs (called on reload or startup)
    void load_clients(std::vector<ClientPricingConfig> clients);

    // Update a single client's tier
    void update_client_tier(const std::string& client_id, TierId tier);

    // Update a single client's alpha score
    void update_client_alpha(const std::string& client_id, double alpha);

    // Generate prices for all active clients for a given pair
    struct ClientPriceResult {
        std::string client_id;
        Price bid;
        Price ask;
        double spread_bps;
        TierId tier;
    };

    std::vector<ClientPriceResult> price_all_clients(
        const std::string& pair,
        double fair_value_mid,
        double volatility,
        double position,
        double pair_alpha,
        double session_multiplier) const;

    // Get a snapshot of all client configs (for API/dashboard)
    std::vector<ClientPricingConfig> get_all_clients() const;

    const ClientPricingConfig* get_client(const std::string& client_id) const;

    size_t client_count() const { return client_count_.load(); }

private:
    const AppConfig& config_;
    SpreadConstructor spread_ctor_;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, ClientPricingConfig> clients_;
    std::atomic<size_t> client_count_{0};
};

} // namespace efx
