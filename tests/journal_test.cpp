#include "wms/journal.h"

#include <gtest/gtest.h>

#include <string>
#include <utility>

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
