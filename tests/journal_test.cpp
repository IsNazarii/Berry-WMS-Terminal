#include "wms/journal.h"

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace wms;

namespace {

// Fixed truck ids, so a failing test prints the same values every run.
const TruckId kTruck = Uuid::generateV7(TimePoint{}, 1, 7);
const TruckId kOtherTruck = Uuid::generateV7(TimePoint{}, 1, 8);

Weighing pallet(std::string crop, int boxes, Weight net, Money pricePerKg = hryvnias(85)) {
    return {
        .cropCode = std::move(crop),
        .gross = net + grams(300) * boxes,
        .boxCount = boxes,
        .tarePerBox = grams(300),
        .pricePerKg = pricePerKg,
    };
}

} // namespace

TEST(Journal, StartsEmpty) {
    const Journal journal;
    EXPECT_EQ(journal.size(), 0U);
    EXPECT_TRUE(journal.balanceOf(42).isZero());
}

TEST(Journal, BalanceIsReceiptsMinusPayments) {
    Journal journal;
    journal.add(Receipt::create(42, Settlement::OnSite, {pallet("raspberry", 10, kilograms(20))})); // 1700 грн
    journal.add(Payment::create(42, hryvnias(1000), PaymentMethod::Cash));

    EXPECT_EQ(journal.balanceOf(42), hryvnias(700));
}

TEST(Journal, BalanceCountsOnlyThatSupplier) {
    Journal journal;
    journal.add(Receipt::create(42, Settlement::OnSite, {pallet("raspberry", 10, kilograms(20))}));
    journal.add(Payment::create(7, hryvnias(500), PaymentMethod::Cash));

    EXPECT_EQ(journal.balanceOf(42), hryvnias(1700));
    EXPECT_EQ(journal.balanceOf(7), -hryvnias(500)); // overpaid: supplier owes us
}

TEST(Journal, WholesaleReceiptCreatesNoDebt) {
    Journal journal;
    journal.add(Receipt::create(42, Settlement::HandedOver, {pallet("raspberry", 10, kilograms(20), Money{})}));
    EXPECT_TRUE(journal.balanceOf(42).isZero());
}

TEST(Journal, SameRecordIsAddedOnlyOnce) {
    // Sync may deliver a record the terminal already has; the UUID makes that harmless.
    Journal journal;
    const Payment payment = Payment::create(42, hryvnias(100), PaymentMethod::Cash);

    EXPECT_TRUE(journal.add(payment));
    EXPECT_FALSE(journal.add(payment));
    EXPECT_EQ(journal.size(), 1U);
    EXPECT_EQ(journal.balanceOf(42), -hryvnias(100));
}

TEST(Journal, EmptyTruckHasNoLoad) {
    const Journal journal;
    EXPECT_EQ(journal.loadOf(kTruck), Cargo{});
}

TEST(Journal, TruckLoadSumsItsAllocations) {
    Journal journal;
    const Receipt receipt = Receipt::create(
        42, Settlement::OnSite, {pallet("raspberry", 80, kilograms(160)), pallet("blueberry", 40, kilograms(70))});
    journal.add(receipt);
    // Blueberry: 40 boxes, 70 kg -> 1.75 kg per box.
    journal.add(Allocation::wholeWeighing(receipt, 0, kTruck));
    journal.add(Allocation::boxesOf(receipt, "blueberry", 10, kTruck));
    journal.add(Allocation::boxesOf(receipt, "blueberry", 30, kOtherTruck)); // another truck

    EXPECT_EQ(journal.loadOf(kTruck), (Cargo{.boxes = 90, .weight = grams(177'500)}));
    EXPECT_EQ(journal.loadOf(kOtherTruck), (Cargo{.boxes = 30, .weight = grams(52'500)}));
}

namespace {

// Raspberry: two pallets, 80 + 20 boxes, 2 kg per box. Blueberry: one pallet of 40 boxes.
Receipt mixedReceipt() {
    return Receipt::create(42, Settlement::OnSite,
                           {pallet("raspberry", 80, kilograms(160)), pallet("raspberry", 20, kilograms(40)),
                            pallet("blueberry", 40, kilograms(70))});
}

bool mentions(const std::vector<std::string>& errors, std::string_view fragment) {
    for (const auto& e : errors) {
        if (e.find(fragment) != std::string::npos) return true;
    }
    return false;
}

} // namespace

TEST(Journal, NothingAllocatedYetLeavesWholeCrop) {
    Journal journal;
    const Receipt receipt = mixedReceipt();
    journal.add(receipt);
    EXPECT_EQ(journal.unallocatedOf(receipt.id(), "raspberry"), (Cargo{.boxes = 100, .weight = kilograms(200)}));
}

TEST(Journal, UnallocatedSubtractsOnlyThatReceiptAndCrop) {
    Journal journal;
    const Receipt receipt = mixedReceipt();
    const Receipt other = mixedReceipt();
    journal.add(receipt);
    journal.add(other);
    journal.add(Allocation::wholeWeighing(receipt, 0, kTruck));         // 80 raspberry boxes
    journal.add(Allocation::boxesOf(receipt, "blueberry", 10, kTruck)); // other crop
    journal.add(Allocation::boxesOf(other, "raspberry", 5, kTruck));    // other receipt

    EXPECT_EQ(journal.unallocatedOf(receipt.id(), "raspberry"), (Cargo{.boxes = 20, .weight = kilograms(40)}));
}

TEST(Journal, UnallocatedOfUnknownReceiptIsProgrammerError) {
    const Journal journal;
    EXPECT_THROW((void)journal.unallocatedOf(mixedReceipt().id(), "raspberry"), std::invalid_argument);
}

TEST(Journal, AllocationWithinWhatIsLeftIsValid) {
    Journal journal;
    const Receipt receipt = mixedReceipt();
    journal.add(receipt);
    journal.add(Allocation::wholeWeighing(receipt, 0, kTruck));

    EXPECT_TRUE(journal.validateAllocation(Allocation::boxesOf(receipt, "raspberry", 20, kOtherTruck)).empty());
}

TEST(Journal, RejectsMoreBoxesThanLeft) {
    Journal journal;
    const Receipt receipt = mixedReceipt();
    journal.add(receipt);
    journal.add(Allocation::wholeWeighing(receipt, 0, kTruck));

    const auto errors = journal.validateAllocation(Allocation::boxesOf(receipt, "raspberry", 21, kOtherTruck));
    EXPECT_TRUE(mentions(errors, "Залишилось лише 20 ящ."));
}

TEST(Journal, RejectsSameWeighingTwice) {
    Journal journal;
    const Receipt receipt = mixedReceipt();
    journal.add(receipt);
    journal.add(Allocation::wholeWeighing(receipt, 1, kTruck));

    const auto errors = journal.validateAllocation(Allocation::wholeWeighing(receipt, 1, kOtherTruck));
    EXPECT_TRUE(mentions(errors, "Зважування 2 вже розподілено"));
}

TEST(Journal, RejectsCancelledWeighing) {
    Journal journal;
    auto mistake = pallet("raspberry", 10, kilograms(20));
    mistake.cancelled = true;
    const Receipt receipt = Receipt::create(42, Settlement::OnSite, {pallet("raspberry", 80, kilograms(160)), mistake});
    journal.add(receipt);

    const auto errors = journal.validateAllocation(Allocation::wholeWeighing(receipt, 1, kTruck));
    EXPECT_TRUE(mentions(errors, "Зважування 2 анульовано"));
}

TEST(Journal, RejectsAllocationOfUnknownReceipt) {
    const Journal journal;
    const auto errors = journal.validateAllocation(Allocation::wholeWeighing(mixedReceipt(), 0, kTruck));
    EXPECT_TRUE(mentions(errors, "Чек не знайдено"));
}

TEST(Journal, KeepsAllocationOwnErrors) {
    Journal journal;
    const Receipt receipt = mixedReceipt();
    journal.add(receipt);

    const auto errors = journal.validateAllocation(Allocation::boxesOf(receipt, "raspberry", 10, TruckId{}));
    EXPECT_TRUE(mentions(errors, "Не обрано фуру"));
}

TEST(Journal, LastBoxesTakeExactlyTheRemainingWeight) {
    // 100 g over 3 boxes: splitting the full crop each time would give 33 + 33 + 33 = 99 g.
    Journal journal;
    const Receipt receipt = Receipt::create(42, Settlement::OnSite, {pallet("strawberry", 3, grams(100))});
    journal.add(receipt);
    for (int i = 0; i < 3; ++i) journal.add(journal.boxesOf(receipt.id(), "strawberry", 1, kTruck));

    EXPECT_EQ(journal.loadOf(kTruck), (Cargo{.boxes = 3, .weight = grams(100)}));
    EXPECT_EQ(journal.unallocatedOf(receipt.id(), "strawberry"), Cargo{});
}

TEST(Journal, BoxesOfUnknownReceiptIsProgrammerError) {
    const Journal journal;
    EXPECT_THROW((void)journal.boxesOf(mixedReceipt().id(), "raspberry", 1, kTruck), std::invalid_argument);
}

TEST(Journal, WarehouseStockSumsAllocationsToIt) {
    Journal journal;
    const Receipt receipt = Receipt::create(
        42, Settlement::OnSite, {pallet("raspberry", 80, kilograms(160)), pallet("blueberry", 40, kilograms(70))});
    journal.add(receipt);
    journal.add(Allocation::wholeWeighing(receipt, 0, ToWarehouse{}));
    journal.add(Allocation::boxesOf(receipt, "blueberry", 10, ToWarehouse{}));
    journal.add(Allocation::boxesOf(receipt, "blueberry", 30, kTruck));

    EXPECT_EQ(journal.stockOf("raspberry"), (Cargo{.boxes = 80, .weight = kilograms(160)}));
    EXPECT_EQ(journal.stockOf("blueberry"), (Cargo{.boxes = 10, .weight = grams(17'500)}));
    EXPECT_EQ(journal.stockOf("strawberry"), Cargo{});
    // The warehouse is not a truck: its berries are no truck's load.
    EXPECT_EQ(journal.loadOf(kTruck), (Cargo{.boxes = 30, .weight = grams(52'500)}));
}

TEST(Journal, StockSpansReceiptsOfAllSuppliers) {
    Journal journal;
    const Receipt first = Receipt::create(42, Settlement::OnSite, {pallet("raspberry", 10, kilograms(20))});
    const Receipt second = Receipt::create(43, Settlement::OnSite, {pallet("raspberry", 5, kilograms(10))});
    journal.add(first);
    journal.add(second);
    journal.add(Allocation::wholeWeighing(first, 0, ToWarehouse{}));
    journal.add(Allocation::wholeWeighing(second, 0, ToWarehouse{}));
    EXPECT_EQ(journal.stockOf("raspberry"), (Cargo{.boxes = 15, .weight = kilograms(30)}));
}

TEST(Journal, PalletPutInTheWarehouseCannotAlsoGoIntoATruck) {
    Journal journal;
    const Receipt receipt = Receipt::create(42, Settlement::OnSite, {pallet("raspberry", 80, kilograms(160))});
    journal.add(receipt);
    journal.add(Allocation::wholeWeighing(receipt, 0, ToWarehouse{}));
    EXPECT_FALSE(journal.validateAllocation(Allocation::wholeWeighing(receipt, 0, kTruck)).empty());
    EXPECT_EQ(journal.unallocatedOf(receipt.id(), "raspberry"), Cargo{});
}

namespace {

// 100 raspberry boxes of 2 kg in the warehouse.
Journal journalWithStock() {
    Journal journal;
    const Receipt receipt = Receipt::create(42, Settlement::OnSite, {pallet("raspberry", 100, kilograms(200))});
    journal.add(receipt);
    journal.add(Allocation::wholeWeighing(receipt, 0, ToWarehouse{}));
    return journal;
}

} // namespace

TEST(Journal, ShipmentMovesStockIntoTheTruck) {
    Journal journal = journalWithStock();
    journal.add(journal.shipmentOf("raspberry", 30, kTruck));

    EXPECT_EQ(journal.stockOf("raspberry"), (Cargo{.boxes = 70, .weight = kilograms(140)}));
    EXPECT_EQ(journal.loadOf(kTruck), (Cargo{.boxes = 30, .weight = kilograms(60)}));
}

TEST(Journal, TruckLoadCountsReceiptsAndWarehouseAlike) {
    Journal journal = journalWithStock();
    const Receipt fresh = Receipt::create(43, Settlement::OnSite, {pallet("raspberry", 10, kilograms(20))});
    journal.add(fresh);
    journal.add(Allocation::wholeWeighing(fresh, 0, kTruck));
    journal.add(journal.shipmentOf("raspberry", 5, kTruck));
    EXPECT_EQ(journal.loadOf(kTruck), (Cargo{.boxes = 15, .weight = kilograms(30)}));
}

TEST(Journal, ShippingTheLastBoxesEmptiesTheStockExactly) {
    Journal journal;
    // 3 boxes weighing 100 g: 33.3 g per box does not divide evenly.
    const Receipt receipt = Receipt::create(42, Settlement::OnSite, {pallet("strawberry", 3, grams(100))});
    journal.add(receipt);
    journal.add(Allocation::wholeWeighing(receipt, 0, ToWarehouse{}));
    journal.add(journal.shipmentOf("strawberry", 1, kTruck));
    journal.add(journal.shipmentOf("strawberry", 2, kTruck));
    EXPECT_EQ(journal.stockOf("strawberry"), Cargo{});
}

TEST(Journal, CannotShipMoreThanIsInStock) {
    const Journal journal = journalWithStock();
    const auto errors = journal.validateShipment(journal.shipmentOf("raspberry", 101, kTruck));
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_NE(errors[0].find("100"), std::string::npos);
    EXPECT_TRUE(journal.validateShipment(journal.shipmentOf("raspberry", 100, kTruck)).empty());
}

TEST(Journal, OfflineOvershipmentIsKeptAsNegativeStock) {
    // Two terminals shipped the same stock while offline: both facts stay,
    // the negative stock tells the logistician to sort it out.
    Journal journal = journalWithStock();
    journal.add(journal.shipmentOf("raspberry", 80, kTruck));
    journal.add(StockShipment::create("raspberry", {.boxes = 80, .weight = kilograms(160)}, kTruck));
    EXPECT_EQ(journal.stockOf("raspberry").boxes, -60);
}
