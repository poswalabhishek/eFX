#include <gtest/gtest.h>
#include "efx/core/config.hpp"
#include "efx/pricing/spread_constructor.hpp"

using namespace efx;

namespace {

AppConfig make_test_config() {
    AppConfig cfg;
    cfg.pairs.push_back(PairConfig{
        .symbol = "EURUSD",
        .pip_size = 0.00001,
        .tick_size = 0.00001,
        .primary_venue = "EBS",
        .base_spread_pips = 0.2,
    });
    cfg.risk = RiskConfig{
        .max_per_pair_usd = 50'000'000,
        .max_total_exposure_usd = 200'000'000,
        .drawdown_warning_usd = -50'000,
        .drawdown_critical_usd = -100'000,
        .drawdown_emergency_usd = -250'000,
        .rolling_window_minutes = 60,
        .rolling_threshold_usd = -100'000,
        .max_hedge_latency_ms = 100,
        .volatility_baseline = 0.065,
        .volatility_coefficient = 1.5,
        .skew_factor = 0.5,
        .max_skew_bps = 2.0,
    };
    cfg.build_indices();
    return cfg;
}

} // namespace

TEST(SpreadTest, BaseSpreadPlatinum) {
    auto cfg = make_test_config();
    SpreadConstructor ctor(cfg);

    auto result = ctor.compute({
        .pair = "EURUSD",
        .fair_value_mid = 1.08500,
        .volatility = 0.065,
        .position = 0.0,
        .tier = TierId::Platinum,
        .client_alpha = 0.0,
        .pair_alpha = 0.0,
        .session_multiplier = 1.0,
        .override_pct = 0.0,
    });

    // Platinum = 1.0x multiplier, base spread 0.2 pips = 0.000002
    // With vol at baseline (1.0x), no skew, no alpha
    EXPECT_LT(result.ask - result.bid, 0.00005);
    EXPECT_GT(result.ask - result.bid, 0.0);
    EXPECT_GT(result.bid, 0.0);
    EXPECT_GT(result.ask, result.bid);
}

TEST(SpreadTest, RestrictedWiderThanPlatinum) {
    auto cfg = make_test_config();
    SpreadConstructor ctor(cfg);

    SpreadConstructor::SpreadInput base_input{
        .pair = "EURUSD",
        .fair_value_mid = 1.08500,
        .volatility = 0.065,
        .position = 0.0,
        .tier = TierId::Platinum,
        .client_alpha = 0.0,
        .pair_alpha = 0.0,
        .session_multiplier = 1.0,
        .override_pct = 0.0,
    };

    auto platinum = ctor.compute(base_input);

    base_input.tier = TierId::Restricted;
    auto restricted = ctor.compute(base_input);

    double platinum_spread = platinum.ask - platinum.bid;
    double restricted_spread = restricted.ask - restricted.bid;

    EXPECT_GT(restricted_spread, platinum_spread * 3.0);
}

TEST(SpreadTest, VolatilityWidensSpread) {
    auto cfg = make_test_config();
    SpreadConstructor ctor(cfg);

    SpreadConstructor::SpreadInput input{
        .pair = "EURUSD",
        .fair_value_mid = 1.08500,
        .volatility = 0.065, // baseline
        .position = 0.0,
        .tier = TierId::Silver,
        .client_alpha = 0.0,
        .pair_alpha = 0.0,
        .session_multiplier = 1.0,
        .override_pct = 0.0,
    };

    auto normal = ctor.compute(input);

    input.volatility = 0.130; // 2x baseline
    auto elevated = ctor.compute(input);

    EXPECT_GT(elevated.ask - elevated.bid, normal.ask - normal.bid);
}

TEST(SpreadTest, InventorySkew) {
    auto cfg = make_test_config();
    SpreadConstructor ctor(cfg);

    SpreadConstructor::SpreadInput input{
        .pair = "EURUSD",
        .fair_value_mid = 1.08500,
        .volatility = 0.065,
        .position = 0.0,
        .tier = TierId::Silver,
        .client_alpha = 0.0,
        .pair_alpha = 0.0,
        .session_multiplier = 1.0,
        .override_pct = 0.0,
    };

    auto flat = ctor.compute(input);
    double flat_mid = (flat.bid + flat.ask) / 2.0;

    // Long position: should skew bid down (discourage more buying)
    input.position = 25'000'000;
    auto long_pos = ctor.compute(input);
    double long_mid = (long_pos.bid + long_pos.ask) / 2.0;

    EXPECT_LT(long_mid, flat_mid);
}
