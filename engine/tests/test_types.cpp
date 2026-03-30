#include <gtest/gtest.h>
#include "efx/core/types.hpp"

using namespace efx;

TEST(TypesTest, CurrencyPairPipConversion) {
    CurrencyPair eurusd{"EURUSD", 0.00001, 0.00001};
    EXPECT_NEAR(eurusd.to_pips(0.00010), 10.0, 0.001);
    EXPECT_NEAR(eurusd.from_pips(5.0), 0.00005, 1e-10);

    CurrencyPair usdjpy{"USDJPY", 0.001, 0.001};
    EXPECT_NEAR(usdjpy.to_pips(0.050), 50.0, 0.001);
    EXPECT_NEAR(usdjpy.from_pips(3.0), 0.003, 1e-10);
}

TEST(TypesTest, CurrencyPairBaseQuote) {
    CurrencyPair eurusd{"EURUSD", 0.00001, 0.00001};
    EXPECT_EQ(eurusd.base(), "EUR");
    EXPECT_EQ(eurusd.quote(), "USD");
}

TEST(TypesTest, SideOperations) {
    EXPECT_EQ(to_string(Side::Buy), "BUY");
    EXPECT_EQ(to_string(Side::Sell), "SELL");
    EXPECT_EQ(opposite(Side::Buy), Side::Sell);
    EXPECT_EQ(opposite(Side::Sell), Side::Buy);
}

TEST(TypesTest, TierMultipliers) {
    EXPECT_DOUBLE_EQ(tier_multiplier(TierId::Platinum), 1.0);
    EXPECT_DOUBLE_EQ(tier_multiplier(TierId::Gold), 1.5);
    EXPECT_DOUBLE_EQ(tier_multiplier(TierId::Silver), 2.5);
    EXPECT_DOUBLE_EQ(tier_multiplier(TierId::Bronze), 4.0);
    EXPECT_DOUBLE_EQ(tier_multiplier(TierId::Restricted), 8.0);
}

TEST(TypesTest, TierNames) {
    EXPECT_EQ(tier_name(TierId::Platinum), "platinum");
    EXPECT_EQ(tier_name(TierId::Restricted), "restricted");
}

TEST(TypesTest, VenueTickMidAndSpread) {
    VenueTick tick;
    tick.bid = 1.08500;
    tick.ask = 1.08510;
    EXPECT_NEAR(tick.mid(), 1.08505, 1e-10);
    EXPECT_NEAR(tick.spread(), 0.00010, 1e-10);
}
