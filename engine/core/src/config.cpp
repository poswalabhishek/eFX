#include "efx/core/config.hpp"

#include <toml++/toml.hpp>
#include <spdlog/spdlog.h>
#include <stdexcept>

namespace efx {

namespace {

std::vector<PairConfig> load_pairs(const std::filesystem::path& path) {
    auto tbl = toml::parse_file(path.string());
    std::vector<PairConfig> pairs;

    if (auto arr = tbl["pairs"].as_array()) {
        for (auto& elem : *arr) {
            auto* t = elem.as_table();
            if (!t) continue;
            PairConfig p;
            p.symbol           = t->get("symbol")->as_string()->get();
            p.pip_size         = t->get("pip_size")->as_floating_point()->get();
            p.tick_size        = t->get("tick_size")->as_floating_point()->get();
            p.primary_venue    = t->get("primary_venue")->as_string()->get();
            p.base_spread_pips = t->get("base_spread_pips")->as_floating_point()->get();
            pairs.push_back(std::move(p));
        }
    }
    spdlog::info("Loaded {} currency pairs from {}", pairs.size(), path.string());
    return pairs;
}

std::vector<VenueConfig> load_venues(const std::filesystem::path& path) {
    auto tbl = toml::parse_file(path.string());
    std::vector<VenueConfig> venues;

    if (auto arr = tbl["venues"].as_array()) {
        for (auto& elem : *arr) {
            auto* t = elem.as_table();
            if (!t) continue;
            VenueConfig v;
            v.id                    = t->get("id")->as_string()->get();
            v.label                 = t->get("label")->as_string()->get();
            v.venue_type            = t->get("venue_type")->as_string()->get();
            v.avg_tick_rate_hz      = t->get("avg_tick_rate_hz")->as_floating_point()
                                        ? t->get("avg_tick_rate_hz")->as_floating_point()->get()
                                        : static_cast<double>(t->get("avg_tick_rate_hz")->as_integer()->get());
            v.base_spread_pips      = t->get("base_spread_pips")->as_floating_point()->get();
            v.staleness_threshold_ms = t->get("staleness_threshold_ms")->as_floating_point()
                                        ? t->get("staleness_threshold_ms")->as_floating_point()->get()
                                        : static_cast<double>(t->get("staleness_threshold_ms")->as_integer()->get());
            v.latency_offset_ms     = t->get("latency_offset_ms")->as_floating_point()
                                        ? t->get("latency_offset_ms")->as_floating_point()->get()
                                        : static_cast<double>(t->get("latency_offset_ms")->as_integer()->get());
            v.reliability           = t->get("reliability")->as_floating_point()->get();
            v.update_rate_ms        = t->get("update_rate_ms")->as_floating_point()
                                        ? t->get("update_rate_ms")->as_floating_point()->get()
                                        : static_cast<double>(t->get("update_rate_ms")->as_integer()->get());
            venues.push_back(std::move(v));
        }
    }
    spdlog::info("Loaded {} venues from {}", venues.size(), path.string());
    return venues;
}

std::vector<TierConfig> load_tiers(const std::filesystem::path& path) {
    auto tbl = toml::parse_file(path.string());
    std::vector<TierConfig> tiers;

    if (auto arr = tbl["tiers"].as_array()) {
        for (auto& elem : *arr) {
            auto* t = elem.as_table();
            if (!t) continue;
            TierConfig tc;
            tc.id          = t->get("id")->as_string()->get();
            tc.label       = t->get("label")->as_string()->get();
            tc.multiplier  = t->get("multiplier")->as_floating_point()->get();
            tc.description = t->get("description")->as_string()->get();
            tiers.push_back(std::move(tc));
        }
    }
    spdlog::info("Loaded {} tiers from {}", tiers.size(), path.string());
    return tiers;
}

std::vector<SessionConfig> load_sessions(const std::filesystem::path& path) {
    auto tbl = toml::parse_file(path.string());
    std::vector<SessionConfig> sessions;

    if (auto arr = tbl["sessions"].as_array()) {
        for (auto& elem : *arr) {
            auto* t = elem.as_table();
            if (!t) continue;
            SessionConfig s;
            s.id                = t->get("id")->as_string()->get();
            s.label             = t->get("label")->as_string()->get();
            s.hub_city          = t->get("hub_city")->as_string()->get();
            s.start_utc         = t->get("start_utc")->as_string()->get();
            s.end_utc           = t->get("end_utc")->as_string()->get();
            s.spread_multiplier = t->get("spread_multiplier")->as_floating_point()->get();

            if (auto fp = t->get("focus_pairs"); fp && fp->as_array()) {
                for (auto& p : *fp->as_array()) {
                    s.focus_pairs.push_back(p.as_string()->get());
                }
            }
            sessions.push_back(std::move(s));
        }
    }
    spdlog::info("Loaded {} sessions from {}", sessions.size(), path.string());
    return sessions;
}

RiskConfig load_risk(const std::filesystem::path& path) {
    auto tbl = toml::parse_file(path.string());
    RiskConfig r{};

    auto get_double = [&](const std::string& section, const std::string& key) -> double {
        auto* node = tbl[section][key].as_floating_point();
        if (node) return node->get();
        auto* inode = tbl[section][key].as_integer();
        if (inode) return static_cast<double>(inode->get());
        throw std::runtime_error("Missing config: " + section + "." + key);
    };

    r.max_per_pair_usd       = get_double("position_limits", "max_per_pair_usd");
    r.max_total_exposure_usd = get_double("position_limits", "max_total_exposure_usd");
    r.drawdown_warning_usd   = get_double("drawdown_thresholds", "warning_usd");
    r.drawdown_critical_usd  = get_double("drawdown_thresholds", "critical_usd");
    r.drawdown_emergency_usd = get_double("drawdown_thresholds", "emergency_usd");
    r.rolling_window_minutes = get_double("drawdown_rolling", "window_minutes");
    r.rolling_threshold_usd  = get_double("drawdown_rolling", "threshold_usd");
    r.max_hedge_latency_ms   = get_double("hedging", "max_hedge_latency_ms");
    r.volatility_baseline    = get_double("spread_widening", "volatility_baseline");
    r.volatility_coefficient = get_double("spread_widening", "volatility_coefficient");
    r.skew_factor            = get_double("inventory_skew", "skew_factor");
    r.max_skew_bps           = get_double("inventory_skew", "max_skew_bps");

    spdlog::info("Loaded risk config from {}", path.string());
    return r;
}

} // anonymous namespace

void AppConfig::build_indices() {
    pair_map.clear();
    venue_map.clear();
    for (const auto& p : pairs) pair_map[p.symbol] = &p;
    for (const auto& v : venues) venue_map[v.id] = &v;
}

AppConfig load_config(const std::filesystem::path& config_dir) {
    spdlog::info("Loading configuration from {}", config_dir.string());

    AppConfig cfg;
    cfg.pairs    = load_pairs(config_dir / "pairs.toml");
    cfg.venues   = load_venues(config_dir / "venues.toml");
    cfg.tiers    = load_tiers(config_dir / "tiers.toml");
    cfg.sessions = load_sessions(config_dir / "sessions.toml");
    cfg.risk     = load_risk(config_dir / "risk.toml");
    cfg.build_indices();

    spdlog::info("Configuration loaded: {} pairs, {} venues, {} tiers, {} sessions",
        cfg.pairs.size(), cfg.venues.size(), cfg.tiers.size(), cfg.sessions.size());

    return cfg;
}

} // namespace efx
