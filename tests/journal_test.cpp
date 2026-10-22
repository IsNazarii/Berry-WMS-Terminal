#include "wms/journal.h"
#include "wms/open_receipt.h"

#include <gtest/gtest.h>

#include <chrono>
#include <optional>
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
    journal.add(Payment::create(42, hryvnias(1000), PaymentMethod::Cash, 1));

    EXPECT_EQ(journal.balanceOf(42), hryvnias(700));
}

TEST(Journal, BalanceCountsOnlyThatSupplier) {
    Journal journal;
    journal.add(Receipt::create(42, Settlement::OnSite, {pallet("raspberry", 10, kilograms(20))}));
    journal.add(Payment::create(7, hryvnias(500), PaymentMethod::Cash, 1));

    EXPECT_EQ(journal.balanceOf(42), hryvnias(1700));
    EXPECT_EQ(journal.balanceOf(7), -hryvnias(500)); // overpaid: supplier owes us
}

TEST(Journal, WholesaleReceiptCreatesNoDebt) {
    Journal journal;
    journal.add(Receipt::create(42, Settlement::HandedOver, {pallet("raspberry", 10, kilograms(20), Money{})}));
    EXPECT_TRUE(journal.balanceOf(42).isZero());
}

TEST(Journal, HistoryListsReceiptsAndPaymentsOfThatSupplierByTime) {
    // Synced records arrive in any order; the cash screen shows them as they happened.
    const TimePoint morning{std::chrono::hours{8}};
    const Receipt receipt{Uuid::generateV7(morning, 1, 1), morning, 42, Settlement::OnSite,
                          {pallet("raspberry", 10, kilograms(20))}};
    const Payment payment{Uuid::generateV7(morning, 1, 2), morning + std::chrono::hours{2}, 42, hryvnias(1000),
                          PaymentMethod::Cash, 1};
    const Payment otherSupplier = Payment::create(7, hryvnias(500), PaymentMethod::Cash, 1);

    Journal journal;
    journal.add(payment);
    journal.add(otherSupplier);
    journal.add(receipt);

    const std::vector<const Transaction*> history = journal.historyOf(42);
    ASSERT_EQ(history.size(), 2U);
    EXPECT_EQ(history[0]->id(), receipt.id());
    EXPECT_EQ(history[1]->id(), payment.id());
    // The balance is the sum of what the history shows.
    EXPECT_EQ(history[0]->balanceEffect() + history[1]->balanceEffect(), journal.balanceOf(42));
}

TEST(Journal, SupplierWithoutRecordsHasEmptyHistory) {
    Journal journal;
    journal.add(Payment::create(7, hryvnias(500), PaymentMethod::Cash, 1));
    EXPECT_TRUE(journal.historyOf(42).empty());
}

TEST(Journal, SameRecordIsAddedOnlyOnce) {
    // Sync may deliver a record the terminal already has; the UUID makes that harmless.
    Journal journal;
    const Payment payment = Payment::create(42, hryvnias(100), PaymentMethod::Cash, 1);

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

namespace {

// A big supplier still unloading: two raspberry pallets on the scale so far.
OpenReceipt unloading() {
    OpenReceipt r = OpenReceipt::open(42);
    r.add(pallet("raspberry", 80, kilograms(160)));
    r.add(pallet("raspberry", 20, kilograms(40)));
    return r;
}

} // namespace

TEST(Journal, PalletOfOpenReceiptGoesIntoTruck) {
    Journal journal;
    const OpenReceipt open = unloading();
    const Allocation a = Allocation::wholeWeighing(open, 0, kTruck);

    EXPECT_TRUE(journal.validateAllocation(a, open).empty());
    journal.add(a);
    EXPECT_EQ(journal.loadOf(kTruck), (Cargo{.boxes = 80, .weight = kilograms(160)}));
    EXPECT_EQ(journal.unallocatedOf(open, "raspberry"), (Cargo{.boxes = 20, .weight = kilograms(40)}));
}

TEST(Journal, PalletOfOpenReceiptCannotGoTwice) {
    Journal journal;
    const OpenReceipt open = unloading();
    journal.add(Allocation::wholeWeighing(open, 1, kTruck));

    const auto errors = journal.validateAllocation(Allocation::wholeWeighing(open, 1, kOtherTruck), open);
    EXPECT_TRUE(mentions(errors, "Зважування 2 вже розподілено"));
}

TEST(Journal, CancelledPalletOfOpenReceiptStaysPut) {
    Journal journal;
    OpenReceipt open = unloading();
    open.cancel(0);

    const auto errors = journal.validateAllocation(Allocation::wholeWeighing(open, 0, kTruck), open);
    EXPECT_TRUE(mentions(errors, "Зважування 1 анульовано"));
}

TEST(Journal, BoxesOfOpenReceiptAreSplitFromWhatIsLeft) {
    Journal journal;
    const OpenReceipt open = unloading();
    journal.add(Allocation::wholeWeighing(open, 0, kTruck));

    const Allocation rest = journal.boxesOf(open, "raspberry", 20, ToWarehouse{});
    EXPECT_EQ(rest.cargo(), (Cargo{.boxes = 20, .weight = kilograms(40)}));
    EXPECT_TRUE(journal.validateAllocation(rest, open).empty());
}

TEST(Journal, AllocationMadeWhileOpenCountsAfterClosing) {
    // Same id and indices after closing, so the pallet already in the truck is not offered again.
    Journal journal;
    const OpenReceipt open = unloading();
    journal.add(Allocation::wholeWeighing(open, 0, kTruck));

    const Receipt closed = open.close(Settlement::OnSite);
    journal.add(closed);
    EXPECT_EQ(journal.unallocatedOf(closed.id(), "raspberry"), (Cargo{.boxes = 20, .weight = kilograms(40)}));
    const auto errors = journal.validateAllocation(Allocation::wholeWeighing(closed, 0, kOtherTruck));
    EXPECT_TRUE(mentions(errors, "Зважування 1 вже розподілено"));
}

TEST(Journal, AllocationOfAnotherOpenReceiptIsProgrammerError) {
    const Journal journal;
    const Allocation a = Allocation::wholeWeighing(unloading(), 0, kTruck);
    EXPECT_THROW((void)journal.validateAllocation(a, unloading()), std::invalid_argument);
}

TEST(Journal, PalletNotYetAllocatedCanBeCancelled) {
    const Journal journal;
    EXPECT_TRUE(journal.validateCancel(unloading(), 1).empty());
}

TEST(Journal, PalletInTruckCannotBeCancelled) {
    Journal journal;
    const OpenReceipt open = unloading();
    journal.add(Allocation::wholeWeighing(open, 0, kTruck));

    const auto errors = journal.validateCancel(open, 0);
    ASSERT_EQ(errors.size(), 1U);
    EXPECT_TRUE(mentions(errors, "Зважування 1 вже розподілено"));
}

TEST(Journal, CancelMustLeaveTheBoxesAlreadyAllocated) {
    // 90 of 100 raspberry boxes went into a truck by count, not by pallet:
    // without the 20-box pallet only 80 would be left to cover them.
    Journal journal;
    const OpenReceipt open = unloading();
    journal.add(journal.boxesOf(open, "raspberry", 90, kTruck));

    EXPECT_TRUE(mentions(journal.validateCancel(open, 1), "розподілено вже 90"));
}

TEST(Journal, CancelIsFineWhileEnoughBoxesStay) {
    Journal journal;
    const OpenReceipt open = unloading();
    journal.add(journal.boxesOf(open, "raspberry", 50, kTruck));

    EXPECT_TRUE(journal.validateCancel(open, 1).empty());
}

TEST(Journal, CancelledPalletCannotBeCancelledAgain) {
    const Journal journal;
    OpenReceipt open = unloading();
    open.cancel(0);
    EXPECT_TRUE(mentions(journal.validateCancel(open, 0), "вже анульовано"));
}

TEST(Journal, KnowsWhereAWholePalletWent) {
    Journal journal;
    const OpenReceipt open = unloading();
    journal.add(Allocation::wholeWeighing(open, 0, kTruck));

    EXPECT_EQ(journal.destinationOf(open.id(), 0), Destination{kTruck});
    EXPECT_EQ(journal.destinationOf(open.id(), 1), std::nullopt);
}

TEST(Journal, BoxesByCountAreNotTiedToAPallet) {
    Journal journal;
    const OpenReceipt open = unloading();
    journal.add(journal.boxesOf(open, "raspberry", 20, ToWarehouse{}));

    EXPECT_EQ(journal.destinationOf(open.id(), 0), std::nullopt);
    EXPECT_EQ(journal.destinationOf(open.id(), 1), std::nullopt);
}

TEST(Journal, ReversedAllocationNoLongerCounts) {
    Journal journal;
    const Receipt receipt = Receipt::create(42, Settlement::OnSite, {pallet("raspberry", 10, kilograms(20)),
                                                                     pallet("raspberry", 5, kilograms(10))});
    journal.add(receipt);
    const Allocation intoTruck = Allocation::wholeWeighing(receipt, 0, kTruck);
    const Allocation intoWarehouse = Allocation::wholeWeighing(receipt, 1, ToWarehouse{});
    journal.add(intoTruck);
    journal.add(intoWarehouse);
    journal.add(AllocationReversal::of(intoTruck));
    journal.add(AllocationReversal::of(intoWarehouse));

    EXPECT_EQ(journal.loadOf(kTruck), Cargo{});
    EXPECT_EQ(journal.stockOf("raspberry"), Cargo{});
    EXPECT_EQ(journal.unallocatedOf(receipt.id(), "raspberry"), (Cargo{.boxes = 15, .weight = kilograms(30)}));
    EXPECT_EQ(journal.destinationOf(receipt.id(), 0), std::nullopt);
    EXPECT_EQ(journal.allocationOf(receipt.id(), 0), nullptr);
    // The pallet is free again: it may go somewhere else.
    EXPECT_TRUE(journal.validateAllocation(Allocation::wholeWeighing(receipt, 0, kOtherTruck)).empty());
}

TEST(Journal, ReversalMayArriveBeforeTheAllocation) {
    // Sync does not keep the order in which another terminal made the records.
    Journal journal;
    const Receipt receipt = Receipt::create(42, Settlement::OnSite, {pallet("raspberry", 10, kilograms(20))});
    const Allocation a = Allocation::wholeWeighing(receipt, 0, kTruck);
    journal.add(AllocationReversal::of(a));
    journal.add(a);
    EXPECT_EQ(journal.loadOf(kTruck), Cargo{});
}

TEST(Journal, FindsTheAllocationOfAWholePallet) {
    Journal journal;
    const OpenReceipt open = unloading();
    const Allocation a = Allocation::wholeWeighing(open, 1, kTruck);
    journal.add(a);
    ASSERT_NE(journal.allocationOf(open.id(), 1), nullptr);
    EXPECT_EQ(journal.allocationOf(open.id(), 1)->id(), a.id());
    EXPECT_EQ(journal.allocationOf(open.id(), 0), nullptr);
}

TEST(Journal, ReversalIsCheckedAgainstTheJournal) {
    Journal journal;
    const OpenReceipt open = unloading();
    const Allocation a = Allocation::wholeWeighing(open, 0, kTruck);
    EXPECT_TRUE(mentions(journal.validateReversal(AllocationReversal::of(a)), "Розподіл не знайдено"));

    journal.add(a);
    const AllocationReversal reversal = AllocationReversal::of(a);
    EXPECT_TRUE(journal.validateReversal(reversal).empty());
    journal.add(reversal);
    EXPECT_TRUE(mentions(journal.validateReversal(AllocationReversal::of(a)), "вже скасовано"));
}

TEST(Journal, PalletShippedOnFromTheWarehouseCannotBeTakenBack) {
    // Its boxes are already in a truck: taking it back would leave the stock negative.
    Journal journal;
    const OpenReceipt open = unloading();
    const Allocation a = Allocation::wholeWeighing(open, 0, ToWarehouse{}); // 80 boxes
    journal.add(a);
    journal.add(journal.shipmentOf("raspberry", 10, kTruck));

    EXPECT_TRUE(mentions(journal.validateReversal(AllocationReversal::of(a)), "На складі лише 70 ящ."));
}

TEST(Journal, FindsClosedReceiptById) {
    Journal journal;
    const Receipt receipt = Receipt::create(42, Settlement::OnSite, {pallet("raspberry", 10, kilograms(20))});
    journal.add(receipt);

    ASSERT_NE(journal.findReceipt(receipt.id()), nullptr);
    EXPECT_EQ(journal.findReceipt(receipt.id())->supplierId(), 42);
    EXPECT_EQ(journal.findReceipt(Uuid::generateV7()), nullptr);
}

TEST(Journal, ListsClosedReceiptsWithBerriesLeftToAllocate) {
    Journal journal;
    const Receipt done = Receipt::create(42, Settlement::OnSite, {pallet("raspberry", 10, kilograms(20))});
    const Receipt partly = Receipt::create(
        43, Settlement::OnSite, {pallet("raspberry", 10, kilograms(20)), pallet("blueberry", 5, kilograms(8))});
    journal.add(done);
    journal.add(partly);
    journal.add(Allocation::wholeWeighing(done, 0, kTruck));
    journal.add(Allocation::wholeWeighing(partly, 0, kTruck)); // blueberry still waits

    const auto waiting = journal.waitingForAllocation();
    ASSERT_EQ(waiting.size(), 1U);
    EXPECT_EQ(waiting[0]->id(), partly.id());
}

TEST(Journal, CancelledLinesDoNotWaitForAllocation) {
    OpenReceipt open = unloading();
    open.cancel(0);
    open.cancel(1);
    Journal journal;
    journal.add(Receipt{open.id(), open.openedAt(), 42, Settlement::OnSite, open.weighings()});

    EXPECT_TRUE(journal.waitingForAllocation().empty());
}

// A crop with records cannot leave the database: old receipts still name it.
TEST(Journal, MentionsACropOfAnyReceiptLine) {
    Journal journal;
    Receipt receipt = Receipt::create(42, Settlement::OnSite, {pallet("raspberry", 10, kilograms(30))});
    journal.add(receipt);
    EXPECT_TRUE(journal.mentionsCrop("raspberry"));
    EXPECT_FALSE(journal.mentionsCrop("strawberry"));
}
