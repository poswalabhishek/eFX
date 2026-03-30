#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <filesystem>

namespace efx {

struct PairConfig {
    std::string symbol;
    double pip_size;
    double tick_size;
    std::string primary_venue;
    double base_spread_pips;
};

struct VenueConfig {
    std::string id;
    std::string label;
    std::string venue_type; // "primary" or "secondary"
    double avg_tick_rate_hz;
    double base_spread_pips;
    double staleness_threshold_ms;
    double latency_offset_ms;
    double reliability;
    double update_rate_ms;
};

struct TierConfig {
    std::string id;
    std::string label;
    double multiplier;
    std::string description;
};

struct SessionConfig {
    std::string id;
    std::string label;
    std::string hub_city;
    std::string start_utc;
    std::string end_utc;
    double spread_multiplier;
    std::vector<std::string> focus_pairs;
};

struct RiskConfig {
    double max_per_pair_usd;
    double max_total_exposure_usd;
    double drawdown_warning_usd;
    double drawdown_critical_usd;
    double drawdown_emergency_usd;
    double rolling_window_minutes;
    double rolling_threshold_usd;
    double max_hedge_latency_ms;
    double volatility_baseline;
    double volatility_coefficient;
    double skew_factor;
    double max_skew_bps;
};

struct AppConfig {
    std::vector<PairConfig> pairs;
    std::vector<VenueConfig> venues;
    std::vector<TierConfig> tiers;
    std::vector<SessionConfig> sessions;
    RiskConfig risk;

    std::unordered_map<std::string, const PairConfig*> pair_map;
    std::unordered_map<std::string, const VenueConfig*> venue_map;

    void build_indices();
};

AppConfig load_config(const std::filesystem::path& config_dir);

} // namespace efx
