#include "wms/journal.h"

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace wms;

namespace {

Weighing pallet(std::string crop, int boxes, Weight net, Money pricePerKg = hryvnias(85)) {
    return {
        .cropCode = std::move(crop),
        .gross = net + grams(300) * boxes,
        .boxCount = boxes,
        .tarePerBox = grams(300),
        .pricePerKg = pricePerKg,
        .priceListVersion = 1,
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
    EXPECT_EQ(journal.loadOf(7), Cargo{});
}

TEST(Journal, TruckLoadSumsItsAllocations) {
    Journal journal;
    const Receipt receipt = Receipt::create(
        42, Settlement::OnSite, {pallet("raspberry", 80, kilograms(160)), pallet("blueberry", 40, kilograms(70))});
    journal.add(receipt);
    // Blueberry: 40 boxes, 70 kg -> 1.75 kg per box.
    journal.add(CrossDockAllocation::wholeWeighing(receipt, 0, 7));
    journal.add(CrossDockAllocation::boxesOf(receipt, "blueberry", 10, 7));
    journal.add(CrossDockAllocation::boxesOf(receipt, "blueberry", 30, 8)); // another truck

    EXPECT_EQ(journal.loadOf(7), (Cargo{.boxes = 90, .weight = grams(177'500)}));
    EXPECT_EQ(journal.loadOf(8), (Cargo{.boxes = 30, .weight = grams(52'500)}));
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
    journal.add(CrossDockAllocation::wholeWeighing(receipt, 0, 7));         // 80 raspberry boxes
    journal.add(CrossDockAllocation::boxesOf(receipt, "blueberry", 10, 7)); // other crop
    journal.add(CrossDockAllocation::boxesOf(other, "raspberry", 5, 7));    // other receipt

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
    journal.add(CrossDockAllocation::wholeWeighing(receipt, 0, 7));

    EXPECT_TRUE(journal.validateAllocation(CrossDockAllocation::boxesOf(receipt, "raspberry", 20, 8)).empty());
}

TEST(Journal, RejectsMoreBoxesThanLeft) {
    Journal journal;
    const Receipt receipt = mixedReceipt();
    journal.add(receipt);
    journal.add(CrossDockAllocation::wholeWeighing(receipt, 0, 7));

    const auto errors = journal.validateAllocation(CrossDockAllocation::boxesOf(receipt, "raspberry", 21, 8));
    EXPECT_TRUE(mentions(errors, "Залишилось лише 20 ящ."));
}

TEST(Journal, RejectsSameWeighingTwice) {
    Journal journal;
    const Receipt receipt = mixedReceipt();
    journal.add(receipt);
    journal.add(CrossDockAllocation::wholeWeighing(receipt, 1, 7));

    const auto errors = journal.validateAllocation(CrossDockAllocation::wholeWeighing(receipt, 1, 8));
    EXPECT_TRUE(mentions(errors, "Зважування 2 вже розподілено"));
}

TEST(Journal, RejectsAllocationOfUnknownReceipt) {
    const Journal journal;
    const auto errors = journal.validateAllocation(CrossDockAllocation::wholeWeighing(mixedReceipt(), 0, 7));
    EXPECT_TRUE(mentions(errors, "Чек не знайдено"));
}

TEST(Journal, KeepsAllocationOwnErrors) {
    Journal journal;
    const Receipt receipt = mixedReceipt();
    journal.add(receipt);

    const auto errors = journal.validateAllocation(CrossDockAllocation::boxesOf(receipt, "raspberry", 10, 0));
    EXPECT_TRUE(mentions(errors, "Не обрано фуру"));
}
