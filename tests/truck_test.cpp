#include "wms/truck.h"

#include <gtest/gtest.h>

#include <stdexcept>

using namespace wms;

namespace {

Truck truckFor(int boxLimit) { return Truck::create("AA1234BB", boxLimit); }

} // namespace

TEST(Truck, CreateGivesEveryTruckItsOwnId) {
    // Trucks are created offline on each terminal, so ids must not clash.
    const Truck a = Truck::create("AA1234BB", 10);
    const Truck b = Truck::create("AA1234BB", 10);
    EXPECT_EQ(a.id().version(), 7);
    EXPECT_NE(a.id(), b.id());
}

TEST(Truck, DetailsAreOptional) {
    const Truck bare = Truck::create("AA1234BB", 10);
    EXPECT_TRUE(bare.details().make.empty());
    EXPECT_TRUE(bare.details().note.empty());

    const Truck described = Truck::create("AA1234BB", 10, {.make = "DAF XF", .note = "Іван, на Київ"});
    EXPECT_EQ(described.details().make, "DAF XF");
    EXPECT_EQ(described.details().note, "Іван, на Київ");
}

TEST(Cargo, AddsUp) {
    Cargo total{.boxes = 10, .weight = kilograms(20)};
    total += {.boxes = 5, .weight = kilograms(8)};
    EXPECT_EQ(total, (Cargo{.boxes = 15, .weight = kilograms(28)}));
}

TEST(Truck, RejectsInvalidConstruction) {
    EXPECT_THROW((Truck{TruckId{}, "AA1234BB", 10}), std::invalid_argument); // nil id
    EXPECT_THROW((void)(Truck::create("", 10)), std::invalid_argument);
    EXPECT_THROW((void)(Truck::create("AA1234BB", 0)), std::invalid_argument); // a truck always has a limit
    EXPECT_THROW((void)(Truck::create("AA1234BB", -1)), std::invalid_argument);
}

TEST(Truck, BoxesLeft) {
    const Truck truck = truckFor(100);
    EXPECT_EQ(truck.boxesLeft({.boxes = 30, .weight = kilograms(120)}), 70);
}

TEST(Truck, IsLimitedByBoxesOnly) {
    // Kilograms are for reference: however heavy, a truck within its boxes is not overloaded.
    const Truck truck = truckFor(100);
    EXPECT_EQ(truck.fillPercent({}), 0);
    EXPECT_EQ(truck.fillPercent({.boxes = 40, .weight = kilograms(9'999)}), 40);
    EXPECT_FALSE(truck.isOverloaded({.boxes = 100, .weight = kilograms(9'999)}));
}

TEST(Truck, OverloadIsReportedNotRejected) {
    // Two offline terminals may load the same truck; facts are kept and flagged for the logistician.
    const Truck truck = truckFor(100);
    const Cargo loaded{.boxes = 110, .weight = kilograms(450)};
    EXPECT_TRUE(truck.isOverloaded(loaded));
    EXPECT_EQ(truck.fillPercent(loaded), 110);
    EXPECT_EQ(truck.boxesLeft(loaded), -10);
}

TEST(Truck, DepartsOnlyOnce) {
    Truck truck = truckFor(100);
    EXPECT_TRUE(truck.acceptsCargo());
    truck.depart();
    EXPECT_EQ(truck.status(), TruckStatus::Departed);
    EXPECT_FALSE(truck.acceptsCargo());
    EXPECT_THROW(truck.depart(), std::logic_error);
}

TEST(Truck, CarriedOverTruckTakesNoMoreCargoInThisSession) {
    Truck truck = truckFor(100);
    truck.carryOver();
    EXPECT_EQ(truck.status(), TruckStatus::CarriedOver);
    EXPECT_FALSE(truck.acceptsCargo());
    EXPECT_THROW(truck.depart(), std::logic_error);
    EXPECT_THROW(truck.carryOver(), std::logic_error);
}

TEST(Truck, RestoredWithItsStatus) {
    // A truck read back from storage keeps whether it left or was carried over.
    const Truck departed{Uuid::generateV7(), "AA1234BB", 10, {}, TruckStatus::Departed};
    EXPECT_EQ(departed.status(), TruckStatus::Departed);
    EXPECT_FALSE(departed.acceptsCargo());
}
