#include "wms/scale.h"

#include <gtest/gtest.h>

#include <stdexcept>

using namespace wms;

TEST(SimulatedScale, ReadsNothingWhileClosed) {
    SimulatedScaleDriver scale{{grams(100)}};
    EXPECT_FALSE(scale.read());
    scale.open();
    EXPECT_EQ(scale.read(), grams(100));
    scale.close();
    EXPECT_FALSE(scale.read());
}

TEST(SimulatedScale, PlaysScriptThenRepeatsLastReading) {
    SimulatedScaleDriver scale{{grams(1), grams(2)}};
    scale.open();
    EXPECT_EQ(scale.read(), grams(1));
    EXPECT_EQ(scale.read(), grams(2));
    EXPECT_EQ(scale.read(), grams(2));
}

TEST(SimulatedScale, WorksThroughInterface) {
    SimulatedScaleDriver simulated{{grams(5)}};
    IScaleDriver& scale = simulated;
    EXPECT_TRUE(scale.open());
    EXPECT_TRUE(scale.isOpen());
    EXPECT_EQ(scale.read(), grams(5));
}

TEST(StabilityDetector, NotStableUntilWindowIsFull) {
    StabilityDetector d{3, grams(10)};
    d.push(grams(1'000));
    d.push(grams(1'000));
    EXPECT_FALSE(d.isStable());
    EXPECT_FALSE(d.stableWeight());
    d.push(grams(1'000));
    EXPECT_TRUE(d.isStable());
}

TEST(StabilityDetector, StableWithinToleranceReturnsRoundedAverage) {
    StabilityDetector d{3, grams(10)};
    d.push(grams(1'000));
    d.push(grams(1'010));
    d.push(grams(1'005));
    EXPECT_EQ(d.stableWeight(), grams(1'005));
}

TEST(StabilityDetector, UnstableWhenSpreadExceedsTolerance) {
    StabilityDetector d{3, grams(10)};
    d.push(grams(1'000));
    d.push(grams(1'011));
    d.push(grams(1'005));
    EXPECT_FALSE(d.isStable());
}

TEST(StabilityDetector, OldReadingsLeaveTheWindow) {
    StabilityDetector d{2, grams(0)};
    d.push(grams(500));
    d.push(grams(900));
    d.push(grams(900));
    EXPECT_EQ(d.stableWeight(), grams(900));
    EXPECT_EQ(d.lastReading(), grams(900));
}

TEST(StabilityDetector, ResetClearsHistory) {
    StabilityDetector d{1, grams(0)};
    d.push(grams(100));
    d.reset();
    EXPECT_FALSE(d.lastReading());
    EXPECT_FALSE(d.isStable());
}

TEST(StabilityDetector, RejectsInvalidSettings) {
    EXPECT_THROW((StabilityDetector{0, grams(1)}), std::invalid_argument);
    EXPECT_THROW((StabilityDetector{3, grams(-1)}), std::invalid_argument);
}
