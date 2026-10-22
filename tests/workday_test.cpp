#include "wms/journal.h"
#include "wms/workday.h"

#include <gtest/gtest.h>

#include <chrono>
#include <optional>
#include <stdexcept>

using namespace wms;
using namespace std::chrono;
using namespace std::chrono_literals;

namespace {

constexpr year_month_day kDate = September / 26 / 2026;
const TimePoint kMidnight = TimePoint{sys_days{kDate}};
constexpr int kBigTruck = 1'200;
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

TEST(WorkDay, RepeatedTruckKeepsPlateAndLimitButGetsANewId) {
    WorkDay yesterday{September / 25 / 2026, kMidnight - 12h, kOksana};
    const Truck original = yesterday.addTruck("BI1234AA", kBigTruck, kDaf);

    WorkDay today = openAtNoon();
    const Truck& repeated = today.repeatTruck(original);
    EXPECT_EQ(repeated.plate(), "BI1234AA");
    EXPECT_EQ(repeated.boxLimit(), kBigTruck);
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
    EXPECT_EQ(day.cancelWeighing(id, 0, Journal{}), std::nullopt); // nothing to take back
    EXPECT_TRUE(day.findReceipt(id)->weighings()[0].cancelled);
}

TEST(WorkDay, CancelledPalletInTruckComesBackWithAReversal) {
    WorkDay day = dayWithRaspberryPrice();
    const Truck& truck = day.addTruck("BI1234AA", kBigTruck);
    const Uuid id = day.openReceipt(42).id();
    day.addWeighing(id, raspberryPallet(), kMidnight + 13h);
    Journal journal;
    const Allocation a = Allocation::wholeWeighing(*day.findReceipt(id), 0, truck.id());
    journal.add(a);

    const auto reversal = day.cancelWeighing(id, 0, journal);
    ASSERT_TRUE(reversal.has_value());
    EXPECT_EQ(reversal->allocationId(), a.id());
    EXPECT_TRUE(day.findReceipt(id)->weighings()[0].cancelled);
}

TEST(WorkDay, PalletInADepartedTruckCannotBeCancelled) {
    WorkDay day = dayWithRaspberryPrice();
    const TruckId truck = day.addTruck("BI1234AA", kBigTruck).id();
    const Uuid id = day.openReceipt(42).id();
    day.addWeighing(id, raspberryPallet(), kMidnight + 13h);
    Journal journal;
    journal.add(Allocation::wholeWeighing(*day.findReceipt(id), 0, truck));
    day.departTruck(truck);

    const auto errors = day.validateCancel(id, 0, journal);
    ASSERT_EQ(errors.size(), 1U);
    EXPECT_NE(errors[0].find("BI1234AA"), std::string::npos);
    EXPECT_THROW((void)day.cancelWeighing(id, 0, journal), std::logic_error);
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

TEST(WorkDay, EverySessionHasItsOwnId) {
    // Stored sessions are told apart by id: two may open on the same date.
    EXPECT_EQ(openAtNoon().id().version(), 7);
    EXPECT_NE(openAtNoon().id(), openAtNoon().id());
}

TEST(WorkDay, RestoredSessionGoesOnWhereItStopped) {
    WorkDay day = dayWithRaspberryPrice();
    const Truck& truck = day.addTruck("BI1234AA", kBigTruck, kDaf);
    const Uuid receiptId = day.openReceipt(42).id();
    day.addWeighing(receiptId, raspberryPallet(), kMidnight + 13h);

    WorkDay restored = WorkDay::restore({
        .id = day.id(),
        .date = day.date(),
        .openedAt = day.openedAt(),
        .openedBy = day.openedBy(),
        .closedAt = std::nullopt,
        .closedBy = std::nullopt,
        .prices = day.prices(),
        .trucks = day.trucks(),
        .amendments = {},
        .openReceipts = day.openReceipts(),
    });
    EXPECT_EQ(restored.id(), day.id());
    EXPECT_TRUE(restored.isOpen());
    EXPECT_EQ(restored.prices().current("raspberry"), day.prices().current("raspberry"));
    ASSERT_NE(restored.findTruck(truck.id()), nullptr);
    ASSERT_NE(restored.findReceipt(receiptId), nullptr);
    // The open receipt keeps taking weighings after the restart.
    EXPECT_EQ(restored.addWeighing(receiptId, raspberryPallet(), kMidnight + 14h), 1U);
}

TEST(WorkDay, RestoredClosedSessionStaysClosed) {
    WorkDay day = openAtNoon();
    day.close(kMidnight + 20h, kPetro);
    const Amendment amendment{.by = kOksana, .at = kMidnight + 30h, .reason = "забули ціну"};
    day.setPrice("raspberry", hryvnias(80), kMidnight + 12h, amendment);

    const WorkDay restored = WorkDay::restore({
        .id = day.id(),
        .date = day.date(),
        .openedAt = day.openedAt(),
        .openedBy = day.openedBy(),
        .closedAt = day.closedAt(),
        .closedBy = day.closedBy(),
        .prices = day.prices(),
        .trucks = day.trucks(),
        .amendments = day.amendments(),
        .openReceipts = {},
    });
    EXPECT_FALSE(restored.isOpen());
    EXPECT_EQ(restored.closedBy(), kPetro);
    ASSERT_EQ(restored.amendments().size(), 1U);
    EXPECT_EQ(restored.amendments()[0].amendment.reason, "забули ціну");
}

TEST(WorkDay, RestoreRejectsInconsistentRecord) {
    const WorkDayRecord base{.id = Uuid::generateV7(), .date = kDate, .openedAt = kMidnight + 12h, .openedBy = kOksana};
    WorkDayRecord halfClosed = base;
    halfClosed.closedAt = kMidnight + 20h; // but nobody closed it
    EXPECT_THROW((void)WorkDay::restore(halfClosed), std::invalid_argument);

    WorkDayRecord closedWithReceipt = base;
    closedWithReceipt.closedAt = kMidnight + 20h;
    closedWithReceipt.closedBy = kOksana;
    closedWithReceipt.openReceipts.push_back(OpenReceipt::open(42));
    EXPECT_THROW((void)WorkDay::restore(closedWithReceipt), std::invalid_argument);

    WorkDayRecord nilId = base;
    nilId.id = Uuid{};
    EXPECT_THROW((void)WorkDay::restore(nilId), std::invalid_argument);
}

// A receipt still being weighed is a record too, even its cancelled lines.
TEST(WorkDay, MentionsACropOfAnOpenReceipt) {
    WorkDay day = openAtNoon();
    const Uuid id = day.openReceipt(42).id();
    EXPECT_FALSE(day.mentionsCrop("raspberry"));
    day.addWeighing(id,
                    {.cropCode = "raspberry",
                     .gross = kilograms(31),
                     .boxCount = 10,
                     .tarePerBox = grams(100),
                     .pricePerKg = hryvnias(85)},
                    kMidnight + 13h);
    (void)day.cancelWeighing(id, 0, Journal{});
    EXPECT_TRUE(day.mentionsCrop("raspberry"));
}

namespace {

Weighing strawberryPallet(int boxes = 60) {
    return {
        .cropCode = "strawberry",
        .gross = kilograms(150),
        .boxCount = boxes,
        .tarePerBox = grams(300),
    };
}

// A session with two trucks and a supplier unloading; pallets are added by the tests.
struct Unloading {
    WorkDay day = dayWithRaspberryPrice();
    TruckId first = day.addTruck("BI1111AA", 200).id();
    TruckId second = day.addTruck("BI2222BB", 200).id();
    Uuid receipt = day.openReceipt(42).id();
    Journal journal;

    // Weighs the pallet and allocates it the way the receiving screen does.
    std::optional<Destination> weigh(const Weighing& pallet, std::optional<Destination> target) {
        const std::size_t index = day.addWeighing(receipt, pallet, kMidnight + 13h);
        const auto allocation = day.autoAllocation(receipt, index, target, journal);
        if (!allocation) return std::nullopt;
        journal.add(*allocation);
        return allocation->destination();
    }
};

} // namespace

TEST(WorkDay, PalletGoesToTheChosenTarget) {
    Unloading u;
    EXPECT_EQ(u.weigh(raspberryPallet(), u.first), Destination{u.first});
    EXPECT_EQ(u.journal.loadOf(u.first).boxes, 80);
}

TEST(WorkDay, PalletCanGoToTheWarehouse) {
    Unloading u;
    EXPECT_EQ(u.weigh(raspberryPallet(), ToWarehouse{}), Destination{ToWarehouse{}});
}

TEST(WorkDay, WithoutATargetThePalletHangs) {
    Unloading u;
    EXPECT_EQ(u.weigh(raspberryPallet(), std::nullopt), std::nullopt);
}

TEST(WorkDay, NextPalletOfTheCropFollowsTheLastOne) {
    // The operator switched the target for strawberries; raspberries of this
    // receipt keep going where they went.
    Unloading u;
    (void)u.weigh(raspberryPallet(), u.first);
    EXPECT_EQ(u.weigh(strawberryPallet(), u.second), Destination{u.second});
    EXPECT_EQ(u.weigh(raspberryPallet(), u.second), Destination{u.first});
}

TEST(WorkDay, LastPalletOfAnotherReceiptDoesNotCount) {
    Unloading u;
    (void)u.weigh(raspberryPallet(), u.first);
    u.receipt = u.day.openReceipt(43).id();
    EXPECT_EQ(u.weigh(raspberryPallet(), u.second), Destination{u.second});
}

TEST(WorkDay, PalletWithoutRoomInTheTruckHangs) {
    // 80 + 80 fit into 200 boxes, the third pallet does not: it waits for the
    // operator instead of spilling into another truck.
    Unloading u;
    (void)u.weigh(raspberryPallet(), u.first);
    (void)u.weigh(raspberryPallet(), u.first);
    EXPECT_EQ(u.weigh(raspberryPallet(), u.first), std::nullopt);
    EXPECT_EQ(u.journal.loadOf(u.first).boxes, 160);
    EXPECT_EQ(u.journal.unallocatedOf(*u.day.findReceipt(u.receipt), "raspberry").boxes, 80);
}

TEST(WorkDay, PalletThatJustFitsGoesIn) {
    Unloading u;
    (void)u.weigh(strawberryPallet(140), u.first);
    EXPECT_EQ(u.weigh(strawberryPallet(60), u.first), Destination{u.first});
}

TEST(WorkDay, AfterTheTruckLeftTheCropGoesToTheTarget) {
    Unloading u;
    (void)u.weigh(raspberryPallet(), u.first);
    u.day.departTruck(u.first);
    EXPECT_EQ(u.weigh(raspberryPallet(), u.second), Destination{u.second});
}

TEST(WorkDay, TargetTruckThatLeftTakesNothing) {
    Unloading u;
    u.day.departTruck(u.first);
    EXPECT_EQ(u.weigh(raspberryPallet(), u.first), std::nullopt);
    EXPECT_EQ(u.weigh(raspberryPallet(), Uuid::generateV7()), std::nullopt); // not a truck of the session
}

TEST(WorkDay, HangingPalletDoesNotChangeWhereTheCropGoes) {
    // The last pallet that went somewhere decides, not the last one weighed.
    Unloading u;
    (void)u.weigh(raspberryPallet(), u.first);
    (void)u.weigh(raspberryPallet(), std::nullopt);
    EXPECT_EQ(u.day.nextDestination(u.receipt, "raspberry", u.second, u.journal), Destination{u.first});
}

TEST(WorkDay, CancelledOrAllocatedPalletIsNotAllocatedAgain) {
    Unloading u;
    (void)u.weigh(raspberryPallet(), u.first);
    EXPECT_EQ(u.day.autoAllocation(u.receipt, 0, u.first, u.journal), std::nullopt);

    const std::size_t wrong = u.day.addWeighing(u.receipt, raspberryPallet(), kMidnight + 14h);
    (void)u.day.cancelWeighing(u.receipt, wrong, u.journal);
    EXPECT_EQ(u.day.autoAllocation(u.receipt, wrong, u.first, u.journal), std::nullopt);
}

TEST(WorkDay, ShowsWhereTheNextPalletWillGo) {
    // The receiving screen says it before the pallet is weighed.
    Unloading u;
    EXPECT_EQ(u.day.nextDestination(u.receipt, "raspberry", std::nullopt, u.journal), std::nullopt);
    EXPECT_EQ(u.day.nextDestination(u.receipt, "raspberry", u.second, u.journal), Destination{u.second});
    (void)u.weigh(raspberryPallet(), u.first);
    EXPECT_EQ(u.day.nextDestination(u.receipt, "raspberry", u.second, u.journal), Destination{u.first});
}

TEST(WorkDay, CancelledPalletNoLongerLeadsTheCrop) {
    // A pallet put into the wrong truck by mistake is cancelled: the crop
    // goes to the target again, not after the mistake.
    Unloading u;
    (void)u.weigh(raspberryPallet(), u.first);
    const auto reversal = u.day.cancelWeighing(u.receipt, 0, u.journal);
    ASSERT_TRUE(reversal.has_value());
    u.journal.add(*reversal);
    EXPECT_EQ(u.journal.loadOf(u.first), Cargo{});
    EXPECT_EQ(u.day.nextDestination(u.receipt, "raspberry", u.second, u.journal), Destination{u.second});
}
