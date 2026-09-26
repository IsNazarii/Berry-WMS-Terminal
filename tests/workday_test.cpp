#include "wms/journal.h"
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
const TruckDetails kDaf{.make = "DAF XF", .note = "Іван, на Київ"};
constexpr OperatorId kOksana = 1;
constexpr OperatorId kPetro = 2;

WorkDay openAtNoon() { return WorkDay{kDate, kMidnight + 12h, kOksana}; }

} // namespace

TEST(WorkDay, IsNamedByItsOpeningDateEvenWhenClosedNextMorning) {
    WorkDay day = openAtNoon();
    EXPECT_TRUE(day.isOpen());
    day.close(kMidnight + 24h + 3h, kOksana); // 03:00 of the next day
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
    WorkDay yesterday{September / 25 / 2026, kMidnight - 12h, kOksana};
    const Truck original = yesterday.addTruck("BI1234AA", kBigTruck, kDaf);

    WorkDay today = openAtNoon();
    const Truck& repeated = today.repeatTruck(original);
    EXPECT_EQ(repeated.plate(), "BI1234AA");
    EXPECT_EQ(repeated.capacity(), kBigTruck);
    EXPECT_EQ(repeated.details(), kDaf); // usually the same driver on the same route
    EXPECT_NE(repeated.id(), original.id()); // a new trip starts empty
    EXPECT_EQ(today.trucks().size(), 1u);
}

TEST(WorkDay, CannotCloseWhileATruckAwaitsADecision) {
    WorkDay day = openAtNoon();
    day.addTruck("BI1234AA", kBigTruck);
    const auto errors = day.validateClose();
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_NE(errors[0].find("BI1234AA"), std::string::npos);
    EXPECT_THROW(day.close(kMidnight + 20h, kOksana), std::logic_error);
    EXPECT_TRUE(day.isOpen());
}

TEST(WorkDay, DepartedTruckStaysInThisSession) {
    WorkDay day = openAtNoon();
    const TruckId id = day.addTruck("BI1234AA", kBigTruck).id();
    day.departTruck(id);
    EXPECT_TRUE(day.validateClose().empty());
    day.close(kMidnight + 20h, kOksana);
    EXPECT_EQ(day.findTruck(id)->status(), TruckStatus::Departed);
    EXPECT_TRUE(day.trucksForNextSession().empty());
}

TEST(WorkDay, CarriedOverTruckKeepsItsIdAndLoadsOnInTheNextSession) {
    WorkDay day = openAtNoon();
    const TruckId id = day.addTruck("BI1234AA", kBigTruck).id();
    day.carryOverTruck(id);
    day.close(kMidnight + 20h, kOksana);
    ASSERT_EQ(day.trucksForNextSession().size(), 1u);

    WorkDay next{September / 27 / 2026, kMidnight + 36h, kOksana};
    for (const Truck& truck : day.trucksForNextSession()) next.adoptTruck(truck);
    // Same id, so everything already allocated to it still counts as its load.
    ASSERT_NE(next.findTruck(id), nullptr);
    EXPECT_TRUE(next.findTruck(id)->acceptsCargo());
}

TEST(WorkDay, CarriedOverTruckKeepsItsDetails) {
    WorkDay day = openAtNoon();
    const TruckId id = day.addTruck("BI1234AA", kBigTruck, kDaf).id();
    day.carryOverTruck(id);

    WorkDay next{September / 27 / 2026, kMidnight + 36h, kOksana};
    next.adoptTruck(day.trucksForNextSession().front());
    EXPECT_EQ(next.findTruck(id)->details(), kDaf);
}

TEST(WorkDay, ClosedSessionRejectsChanges) {
    WorkDay day = openAtNoon();
    day.close(kMidnight + 20h, kOksana);
    EXPECT_THROW(day.close(kMidnight + 21h, kOksana), std::logic_error);
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

TEST(WorkDay, RemembersWhoOpenedAndWhoClosed) {
    WorkDay day = openAtNoon();
    EXPECT_EQ(day.openedBy(), kOksana);
    EXPECT_FALSE(day.closedBy().has_value());
    day.close(kMidnight + 20h, kPetro);
    EXPECT_EQ(day.closedBy(), kPetro);
}

TEST(WorkDay, ClosedSessionAcceptsAmendmentsAndLogsThem) {
    WorkDay day = openAtNoon();
    day.close(kMidnight + 20h, kOksana);

    const Amendment raise{.by = kPetro, .at = kMidnight + 34h, .reason = "Забули внести підняття ціни о 18:00"};
    day.setPrice("raspberry", hryvnias(90), kMidnight + 18h, raise);
    const Truck& late = day.addTruck("BI1234AA", kBigTruck, raise);
    day.departTruck(late.id(), raise);

    EXPECT_EQ(day.prices().priceAt("raspberry", kMidnight + 19h), hryvnias(90));
    EXPECT_EQ(day.findTruck(late.id())->status(), TruckStatus::Departed);
    ASSERT_EQ(day.amendments().size(), 3u);
    const AmendmentRecord& first = day.amendments().front();
    EXPECT_EQ(first.amendment.by, kPetro);
    EXPECT_EQ(first.amendment.reason, raise.reason);
    // What changed is written down too, not only why.
    EXPECT_NE(first.change.find("raspberry"), std::string::npos);
    EXPECT_NE(day.amendments()[1].change.find("BI1234AA"), std::string::npos);
    EXPECT_FALSE(day.isOpen()); // amending does not reopen the session
}

TEST(WorkDay, AmendmentNeedsOperatorAndReason) {
    WorkDay day = openAtNoon();
    day.close(kMidnight + 20h, kOksana);
    const Amendment noReason{.by = kPetro, .at = kMidnight + 34h, .reason = "  "};
    const Amendment noOperator{.by = 0, .at = kMidnight + 34h, .reason = "Причина"};
    EXPECT_THROW(day.setPrice("raspberry", hryvnias(90), kMidnight + 18h, noReason), std::invalid_argument);
    EXPECT_THROW((void)day.addTruck("BI1234AA", kBigTruck, noOperator), std::invalid_argument);
    EXPECT_TRUE(day.amendments().empty());
    EXPECT_TRUE(day.trucks().empty());
}

TEST(WorkDay, FailedAmendmentIsNotLogged) {
    WorkDay day = openAtNoon();
    day.close(kMidnight + 20h, kOksana);
    const Amendment fix{.by = kPetro, .at = kMidnight + 34h, .reason = "Причина"};
    const Truck stranger = Truck::create("BI5678BB", kBigTruck);
    EXPECT_THROW(day.departTruck(stranger.id(), fix), std::invalid_argument);
    EXPECT_TRUE(day.amendments().empty());
}

namespace {

Weighing raspberryPallet() {
    return {
        .cropCode = "raspberry",
        .gross = kilograms(185),
        .boxCount = 80,
        .tarePerBox = grams(300),
        .palletTare = kilograms(1),
    };
}

WorkDay dayWithRaspberryPrice() {
    WorkDay day = openAtNoon();
    day.setPrice("raspberry", hryvnias(85), kMidnight + 12h);
    return day;
}

} // namespace

TEST(WorkDay, OneOpenReceiptPerSupplier) {
    // The supplier came back with more pallets: continue the same receipt.
    WorkDay day = dayWithRaspberryPrice();
    const Uuid first = day.openReceipt(42).id();
    EXPECT_EQ(day.openReceipt(42).id(), first);
    EXPECT_NE(day.openReceipt(43).id(), first);
    EXPECT_EQ(day.openReceipts().size(), 2U);
}

TEST(WorkDay, WeighingTakesTheDayPriceOfThatMoment) {
    WorkDay day = dayWithRaspberryPrice();
    day.setPrice("raspberry", hryvnias(95), kMidnight + 15h);
    const Uuid id = day.openReceipt(42).id();

    day.addWeighing(id, raspberryPallet(), kMidnight + 14h);
    day.addWeighing(id, raspberryPallet(), kMidnight + 16h);

    const auto& lines = day.findReceipt(id)->weighings();
    EXPECT_EQ(lines[0].pricePerKg, hryvnias(85));
    EXPECT_EQ(lines[1].pricePerKg, hryvnias(95));
}

TEST(WorkDay, ManualPriceIsKept) {
    WorkDay day = dayWithRaspberryPrice();
    const Uuid id = day.openReceipt(42).id();
    auto raised = raspberryPallet();
    raised.pricePerKg = hryvnias(90);
    raised.priceSource = PriceSource::Manual;

    day.addWeighing(id, raised, kMidnight + 13h);
    EXPECT_EQ(day.findReceipt(id)->weighings()[0].pricePerKg, hryvnias(90));
}

TEST(WorkDay, WeighingWithoutDayPriceHasNoPrice) {
    // Allowed: a wholesale receipt needs no price; a retail one will not close.
    WorkDay day = openAtNoon();
    const Uuid id = day.openReceipt(42).id();
    day.addWeighing(id, raspberryPallet(), kMidnight + 13h);
    EXPECT_TRUE(day.findReceipt(id)->weighings()[0].pricePerKg.isZero());
}

TEST(WorkDay, CancelWeighingKeepsTheLine) {
    WorkDay day = dayWithRaspberryPrice();
    const Uuid id = day.openReceipt(42).id();
    day.addWeighing(id, raspberryPallet(), kMidnight + 13h);
    day.cancelWeighing(id, 0, Journal{});
    EXPECT_TRUE(day.findReceipt(id)->weighings()[0].cancelled);
}

TEST(WorkDay, PalletInTruckCannotBeCancelled) {
    WorkDay day = dayWithRaspberryPrice();
    const Uuid id = day.openReceipt(42).id();
    day.addWeighing(id, raspberryPallet(), kMidnight + 13h);
    Journal journal;
    journal.add(Allocation::wholeWeighing(*day.findReceipt(id), 0, Uuid::generateV7()));

    EXPECT_THROW(day.cancelWeighing(id, 0, journal), std::logic_error);
    EXPECT_FALSE(day.findReceipt(id)->weighings()[0].cancelled);
}

TEST(WorkDay, ClosedReceiptLeavesTheOpenList) {
    WorkDay day = dayWithRaspberryPrice();
    const Uuid id = day.openReceipt(42).id();
    day.addWeighing(id, raspberryPallet(), kMidnight + 13h);

    const Receipt receipt = day.closeReceipt(id, Settlement::OnSite);
    EXPECT_EQ(receipt.id(), id);
    EXPECT_EQ(receipt.amount(), hryvnias(85 * 160));
    EXPECT_EQ(day.findReceipt(id), nullptr);
    EXPECT_NE(day.openReceipt(42).id(), id); // the next visit is a new receipt
}

TEST(WorkDay, ReceiptThatCannotCloseStaysOpen) {
    WorkDay day = dayWithRaspberryPrice();
    const Uuid id = day.openReceipt(42).id();
    EXPECT_THROW((void)day.closeReceipt(id, Settlement::OnSite), std::logic_error);
    EXPECT_NE(day.findReceipt(id), nullptr);
}

TEST(WorkDay, UnknownReceiptIsProgrammerError) {
    WorkDay day = dayWithRaspberryPrice();
    EXPECT_THROW(day.addWeighing(Uuid::generateV7(), raspberryPallet(), kMidnight + 13h), std::invalid_argument);
}

TEST(WorkDay, CannotCloseWithOpenReceipts) {
    WorkDay day = dayWithRaspberryPrice();
    (void)day.openReceipt(42);
    const auto errors = day.validateClose();
    ASSERT_EQ(errors.size(), 1U);
    EXPECT_NE(errors[0].find("#42"), std::string::npos);
}

TEST(WorkDay, ClosedSessionOpensNoReceipts) {
    WorkDay day = openAtNoon();
    day.close(kMidnight + 20h, kOksana);
    EXPECT_THROW((void)day.openReceipt(42), std::logic_error);
}
