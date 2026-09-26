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
