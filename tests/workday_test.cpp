#include "wms/workday.h"

#include <gtest/gtest.h>

#include <chrono>
#include <stdexcept>

using namespace wms;
using namespace std::chrono;
using namespace std::chrono_literals;

namespace {

constexpr year_month_day kDate = September / 26 / 2026;
const TimePoint kMidnight = TimePoint{sys_days{kDate}};
const Cargo kBigTruck{.boxes = 1'200, .weight = kilograms(2'000)};

WorkDay openAtNoon() { return WorkDay{kDate, kMidnight + 12h}; }

} // namespace

TEST(WorkDay, IsNamedByItsOpeningDateEvenWhenClosedNextMorning) {
    WorkDay day = openAtNoon();
    EXPECT_TRUE(day.isOpen());
    day.close(kMidnight + 24h + 3h); // 03:00 of the next day
    EXPECT_FALSE(day.isOpen());
    EXPECT_EQ(day.date(), kDate);
    EXPECT_EQ(day.closedAt(), kMidnight + 27h);
}

TEST(WorkDay, HasItsOwnPrices) {
    WorkDay day = openAtNoon();
    EXPECT_FALSE(day.prices().current("raspberry").has_value()); // a new session starts empty
    day.setPrice("raspberry", hryvnias(85), kMidnight + 12h);
    EXPECT_EQ(day.prices().current("raspberry"), hryvnias(85));
}

TEST(WorkDay, RepeatedTruckKeepsPlateAndLimitsButGetsANewId) {
    WorkDay yesterday{September / 25 / 2026, kMidnight - 12h};
    const Truck original = yesterday.addTruck("BI1234AA", kBigTruck);

    WorkDay today = openAtNoon();
    const Truck& repeated = today.repeatTruck(original);
    EXPECT_EQ(repeated.plate(), "BI1234AA");
    EXPECT_EQ(repeated.capacity(), kBigTruck);
    EXPECT_NE(repeated.id(), original.id()); // a new trip starts empty
    EXPECT_EQ(today.trucks().size(), 1u);
}

TEST(WorkDay, CannotCloseWhileATruckAwaitsADecision) {
    WorkDay day = openAtNoon();
    day.addTruck("BI1234AA", kBigTruck);
    const auto errors = day.validateClose();
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_NE(errors[0].find("BI1234AA"), std::string::npos);
    EXPECT_THROW(day.close(kMidnight + 20h), std::logic_error);
    EXPECT_TRUE(day.isOpen());
}

TEST(WorkDay, DepartedTruckStaysInThisSession) {
    WorkDay day = openAtNoon();
    const TruckId id = day.addTruck("BI1234AA", kBigTruck).id();
    day.departTruck(id);
    EXPECT_TRUE(day.validateClose().empty());
    day.close(kMidnight + 20h);
    EXPECT_EQ(day.findTruck(id)->status(), TruckStatus::Departed);
    EXPECT_TRUE(day.trucksForNextSession().empty());
}

TEST(WorkDay, CarriedOverTruckKeepsItsIdAndLoadsOnInTheNextSession) {
    WorkDay day = openAtNoon();
    const TruckId id = day.addTruck("BI1234AA", kBigTruck).id();
    day.carryOverTruck(id);
    day.close(kMidnight + 20h);
    ASSERT_EQ(day.trucksForNextSession().size(), 1u);

    WorkDay next{September / 27 / 2026, kMidnight + 36h};
    for (const Truck& truck : day.trucksForNextSession()) next.adoptTruck(truck);
    // Same id, so everything already allocated to it still counts as its load.
    ASSERT_NE(next.findTruck(id), nullptr);
    EXPECT_TRUE(next.findTruck(id)->acceptsCargo());
}

TEST(WorkDay, ClosedSessionRejectsChanges) {
    WorkDay day = openAtNoon();
    day.close(kMidnight + 20h);
    EXPECT_THROW(day.close(kMidnight + 21h), std::logic_error);
    EXPECT_THROW((void)day.addTruck("BI1234AA", kBigTruck), std::logic_error);
    EXPECT_THROW(day.setPrice("raspberry", hryvnias(85), kMidnight + 21h), std::logic_error);
}

TEST(WorkDay, RejectsUnknownTruck) {
    WorkDay day = openAtNoon();
    const Truck stranger = Truck::create("BI5678BB", kBigTruck);
    EXPECT_EQ(day.findTruck(stranger.id()), nullptr);
    EXPECT_THROW(day.departTruck(stranger.id()), std::invalid_argument);
    EXPECT_THROW(day.carryOverTruck(stranger.id()), std::invalid_argument);
}
