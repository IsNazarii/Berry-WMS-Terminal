#include "wms/pricing.h"

#include <gtest/gtest.h>

#include <chrono>
#include <stdexcept>

using namespace wms;
using namespace std::chrono_literals;

namespace {

// Session day at midnight; hours are added to get times of the day.
const TimePoint kDay = TimePoint{std::chrono::sys_days{std::chrono::September / 26 / 2026}};

} // namespace

TEST(DayPrices, EmptyHasNoPrice) {
    const DayPrices prices;
    EXPECT_FALSE(prices.current("raspberry").has_value());
    EXPECT_TRUE(prices.history().empty());
}

TEST(DayPrices, CurrentIsTheLatestChange) {
    DayPrices prices;
    prices.set("raspberry", hryvnias(85), kDay + 12h);
    prices.set("raspberry", hryvnias(90), kDay + 15h);
    EXPECT_EQ(prices.current("raspberry"), hryvnias(90));
    EXPECT_EQ(prices.history().size(), 2u);
}

TEST(DayPrices, WeighingGetsThePriceInEffectAtItsTime) {
    DayPrices prices;
    prices.set("raspberry", hryvnias(85), kDay + 12h);
    prices.set("raspberry", hryvnias(90), kDay + 15h);
    EXPECT_FALSE(prices.priceAt("raspberry", kDay + 11h).has_value()); // before the first price
    EXPECT_EQ(prices.priceAt("raspberry", kDay + 14h), hryvnias(85));
    EXPECT_EQ(prices.priceAt("raspberry", kDay + 15h), hryvnias(90));  // the change applies at once
}

TEST(DayPrices, CropsAreIndependent) {
    DayPrices prices;
    prices.set("raspberry", hryvnias(85), kDay + 12h);
    prices.set("blueberry", hryvnias(120), kDay + 13h);
    EXPECT_EQ(prices.current("raspberry"), hryvnias(85));
    EXPECT_EQ(prices.current("blueberry"), hryvnias(120));
    EXPECT_FALSE(prices.current("strawberry").has_value());
}

TEST(DayPrices, OrderOfRecordingDoesNotMatterOnlyTime) {
    // Changes may arrive out of order, e.g. from the other terminal after sync.
    DayPrices prices;
    prices.set("raspberry", hryvnias(90), kDay + 15h);
    prices.set("raspberry", hryvnias(85), kDay + 12h);
    EXPECT_EQ(prices.priceAt("raspberry", kDay + 14h), hryvnias(85));
    EXPECT_EQ(prices.current("raspberry"), hryvnias(90));
}

TEST(DayPrices, LaterRecordWinsAtTheSameTime) {
    // The operator fixes a typo within the same second.
    DayPrices prices;
    prices.set("raspberry", hryvnias(58), kDay + 12h);
    prices.set("raspberry", hryvnias(85), kDay + 12h);
    EXPECT_EQ(prices.current("raspberry"), hryvnias(85));
}

TEST(DayPrices, RejectsInvalidChanges) {
    DayPrices prices;
    EXPECT_THROW(prices.set("raspberry", Money{}, kDay), std::invalid_argument);
    EXPECT_THROW(prices.set("raspberry", -hryvnias(1), kDay), std::invalid_argument);
    EXPECT_THROW(prices.set("", hryvnias(85), kDay), std::invalid_argument);
    EXPECT_TRUE(prices.history().empty());
}
