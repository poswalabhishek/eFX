#include "efx/pricing/client_pricer.hpp"
#include <spdlog/spdlog.h>

namespace efx {

ClientPricer::ClientPricer(const AppConfig& config)
    : config_(config), spread_ctor_(config) {}

void ClientPricer::load_clients(std::vector<ClientPricingConfig> clients) {
    std::lock_guard<std::mutex> lock(mutex_);
    clients_.clear();
    for (auto& c : clients) {
        clients_[c.client_id] = std::move(c);
    }
    client_count_.store(clients_.size());
    spdlog::info("ClientPricer: loaded {} clients", clients_.size());
}

void ClientPricer::update_client_tier(const std::string& client_id, TierId tier) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = clients_.find(client_id);
    if (it != clients_.end()) {
        it->second.default_tier = tier;
        spdlog::info("ClientPricer: updated {} tier to {}", client_id, tier_name(tier));
    }
}

void ClientPricer::update_client_alpha(const std::string& client_id, double alpha) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = clients_.find(client_id);
    if (it != clients_.end()) {
        it->second.alpha_score = alpha;
    }
}

std::vector<ClientPricer::ClientPriceResult> ClientPricer::price_all_clients(
    const std::string& pair,
    double fair_value_mid,
    double volatility,
    double position,
    double pair_alpha,
    double session_multiplier) const {

    std::vector<ClientPriceResult> results;
    std::lock_guard<std::mutex> lock(mutex_);

    results.reserve(clients_.size());
    for (const auto& [cid, client] : clients_) {
        if (!client.is_active) continue;

        TierId tier = client.tier_for_pair(pair);
        double override_pct = client.override_pct_for_pair(pair);

        SpreadConstructor::SpreadInput input{
            .pair = pair,
            .fair_value_mid = fair_value_mid,
            .volatility = volatility,
            .position = position,
            .tier = tier,
            .client_alpha = client.alpha_score,
            .pair_alpha = pair_alpha,
            .session_multiplier = session_multiplier,
            .override_pct = override_pct,
        };

        auto spread = spread_ctor_.compute(input);

        results.push_back({
            .client_id = cid,
            .bid = spread.bid,
            .ask = spread.ask,
            .spread_bps = spread.spread_bps,
            .tier = tier,
        });
    }

    return results;
}

std::vector<ClientPricingConfig> ClientPricer::get_all_clients() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ClientPricingConfig> result;
    result.reserve(clients_.size());
    for (const auto& [_, c] : clients_) {
        result.push_back(c);
    }
    return result;
}

const ClientPricingConfig* ClientPricer::get_client(const std::string& client_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = clients_.find(client_id);
    return (it != clients_.end()) ? &it->second : nullptr;
}

} // namespace efx
