#include "wms/scale.h"

#include "scale_reading_print.h"

#include <gtest/gtest.h>

#include <stdexcept>

using namespace wms;

TEST(SimulatedScale, ReadsNothingWhileClosed) {
    SimulatedScaleDriver scale{{grams(100)}};
    EXPECT_FALSE(scale.read());
    scale.open();
    EXPECT_EQ(scale.read()->weight, grams(100));
    scale.close();
    EXPECT_FALSE(scale.read());
}

TEST(SimulatedScale, PlaysScriptThenRepeatsLastReading) {
    SimulatedScaleDriver scale{{grams(1), grams(2)}};
    scale.open();
    EXPECT_EQ(scale.read()->weight, grams(1));
    EXPECT_EQ(scale.read()->weight, grams(2));
    EXPECT_EQ(scale.read()->weight, grams(2));
}

TEST(SimulatedScale, WorksThroughInterface) {
    SimulatedScaleDriver simulated{{grams(5)}};
    IScaleDriver& scale = simulated;
    EXPECT_TRUE(scale.open());
    EXPECT_TRUE(scale.isOpen());
    EXPECT_EQ(scale.read()->weight, grams(5));
}

TEST(SimulatedScale, SettlesAfterThreeEqualReadings) {
    SimulatedScaleDriver scale{{grams(500), grams(900)}};
    scale.open();
    EXPECT_EQ(scale.read(), (ScaleReading{grams(500), false, false, false}));
    EXPECT_EQ(scale.read(), (ScaleReading{grams(900), false, false, false}));
    EXPECT_EQ(scale.read(), (ScaleReading{grams(900), false, false, false}));
    EXPECT_EQ(scale.read(), (ScaleReading{grams(900), true, false, false}));
}

TEST(SimulatedScale, NewScriptUnsettlesUntilItRepeats) {
    SimulatedScaleDriver scale{{grams(0)}};
    scale.open();
    for (int i = 0; i < 3; ++i) (void)scale.read();
    EXPECT_EQ(scale.read(), (ScaleReading{grams(0), true, true, false}));

    scale.setScript({grams(16'600)});
    EXPECT_FALSE(scale.read()->stable);
    EXPECT_FALSE(scale.read()->stable);
    EXPECT_EQ(scale.read(), (ScaleReading{grams(16'600), true, false, false}));
}

TEST(SimulatedScale, ReopenedScaleSettlesAgain) {
    SimulatedScaleDriver scale{{grams(700)}};
    scale.open();
    for (int i = 0; i < 3; ++i) (void)scale.read();
    scale.close();
    scale.open();
    EXPECT_FALSE(scale.read()->stable);
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
