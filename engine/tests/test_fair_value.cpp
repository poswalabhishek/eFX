#include <gtest/gtest.h>
#include "efx/core/config.hpp"
#include "efx/market_data/venue_book.hpp"
#include "efx/pricing/fair_value.hpp"

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
    cfg.venues.push_back(VenueConfig{
        .id = "EBS",
        .label = "EBS",
        .venue_type = "primary",
        .avg_tick_rate_hz = 100,
        .base_spread_pips = 0.15,
        .staleness_threshold_ms = 500,
        .latency_offset_ms = 2,
        .reliability = 0.98,
        .update_rate_ms = 5,
    });
    cfg.venues.push_back(VenueConfig{
        .id = "Reuters",
        .label = "Reuters",
        .venue_type = "primary",
        .avg_tick_rate_hz = 80,
        .base_spread_pips = 0.25,
        .staleness_threshold_ms = 500,
        .latency_offset_ms = 3,
        .reliability = 0.97,
        .update_rate_ms = 5,
    });
    cfg.build_indices();
    return cfg;
}

} // namespace

TEST(FairValueTest, ConvergesToMid) {
    auto cfg = make_test_config();
    VenueBookManager book(cfg);
    FairValueEngine engine(cfg);

    auto now = Timestamp::clock::now();
    auto wall = WallClock::clock::now();

    // Send same-ish ticks from two venues
    book.on_tick(VenueTick{
        .venue = VenueId{"EBS"}, .pair = "EURUSD",
        .bid = 1.08500, .ask = 1.08510,
        .bid_size = 2e6, .ask_size = 2e6,
        .timestamp = now, .wall_time = wall,
    });
    book.on_tick(VenueTick{
        .venue = VenueId{"Reuters"}, .pair = "EURUSD",
        .bid = 1.08502, .ask = 1.08512,
        .bid_size = 1.5e6, .ask_size = 1.5e6,
        .timestamp = now, .wall_time = wall,
    });

    engine.update(book, now);

    auto fv = engine.get("EURUSD");
    ASSERT_NE(fv, nullptr);
    EXPECT_GT(fv->mid, 1.08500);
    EXPECT_LT(fv->mid, 1.08520);
    EXPECT_GT(fv->confidence, 0.0);
    EXPECT_EQ(fv->num_active_venues, 2);
}

TEST(FairValueTest, SmoothesTicks) {
    auto cfg = make_test_config();
    VenueBookManager book(cfg);
    FairValueEngine engine(cfg);
    engine.set_alpha(0.3);

    auto now = Timestamp::clock::now();
    auto wall = WallClock::clock::now();

    // First tick establishes baseline
    book.on_tick(VenueTick{
        .venue = VenueId{"EBS"}, .pair = "EURUSD",
        .bid = 1.08500, .ask = 1.08510,
        .bid_size = 2e6, .ask_size = 2e6,
        .timestamp = now, .wall_time = wall,
    });
    engine.update(book, now);
    double first_mid = engine.get("EURUSD")->mid;

    // Spike tick -- should be dampened
    now += std::chrono::milliseconds(1);
    book.on_tick(VenueTick{
        .venue = VenueId{"EBS"}, .pair = "EURUSD",
        .bid = 1.08600, .ask = 1.08610,
        .bid_size = 2e6, .ask_size = 2e6,
        .timestamp = now, .wall_time = wall,
    });
    engine.update(book, now);
    double second_mid = engine.get("EURUSD")->mid;

    // Should move towards spike but not jump all the way
    EXPECT_GT(second_mid, first_mid);
    EXPECT_LT(second_mid, 1.08605);
}
