#include "wms/allocation.h"

#include <gtest/gtest.h>

#include <stdexcept>
#include <utility>

using namespace wms;

namespace {

Weighing pallet(std::string crop, int boxes, Weight net) {
    return {
        .cropCode = std::move(crop),
        .gross = net + grams(300) * boxes + kilograms(25),
        .boxCount = boxes,
        .tarePerBox = grams(300),
        .palletTare = kilograms(25),
        .pricePerKg = hryvnias(85),
        .priceListVersion = 1,
    };
}

// Two raspberry pallets (80 + 33 boxes) and one blueberry pallet.
Receipt sampleReceipt() {
    return Receipt::create(42, Settlement::OnSite, {pallet("raspberry", 80, kilograms(160)), pallet("raspberry", 33, grams(66'000)),
                                pallet("blueberry", 40, kilograms(70))});
}

} // namespace

TEST(CrossDockAllocation, WholeWeighingTakesExactCargo) {
    const Receipt receipt = sampleReceipt();
    const auto a = CrossDockAllocation::wholeWeighing(receipt, 1, 7);

    EXPECT_EQ(a.receiptId(), receipt.id());
    EXPECT_EQ(a.truckId(), 7);
    EXPECT_EQ(a.cropCode(), "raspberry");
    EXPECT_EQ(a.cargo(), (Cargo{.boxes = 33, .weight = grams(66'000)}));
    EXPECT_EQ(a.weighingIndex(), 1U);
    EXPECT_TRUE(a.isValid());
}

TEST(CrossDockAllocation, WholeWeighingRejectsBadIndex) {
    EXPECT_THROW((void)CrossDockAllocation::wholeWeighing(sampleReceipt(), 3, 7), std::out_of_range);
}

TEST(CrossDockAllocation, BoxesOfCropGetProportionalWeight) {
    // Raspberry in the receipt: 113 boxes, 226 kg -> 2 kg per box.
    const auto a = CrossDockAllocation::boxesOf(sampleReceipt(), "raspberry", 50, 7);
    EXPECT_EQ(a.cargo(), (Cargo{.boxes = 50, .weight = kilograms(100)}));
    EXPECT_FALSE(a.weighingIndex().has_value());
}

TEST(CrossDockAllocation, ProportionalWeightIsRoundedToGrams) {
    // 100 g over 3 boxes -> 33.3 g per box.
    const Receipt receipt = Receipt::create(42, Settlement::OnSite, {pallet("strawberry", 3, grams(100))});
    EXPECT_EQ(CrossDockAllocation::boxesOf(receipt, "strawberry", 1, 7).cargo().weight, grams(33));
    EXPECT_EQ(CrossDockAllocation::boxesOf(receipt, "strawberry", 2, 7).cargo().weight, grams(67));
}

TEST(CrossDockAllocation, BoxesOfSplitWhatIsStillAvailable) {
    // 100 g over 3 boxes, one box already gone (33 g): the other two share the remaining 67 g.
    const Receipt receipt = Receipt::create(42, Settlement::OnSite, {pallet("strawberry", 3, grams(100))});
    const Cargo available{.boxes = 2, .weight = grams(67)};
    EXPECT_EQ(CrossDockAllocation::boxesOf(receipt, "strawberry", 1, 7, available).cargo().weight, grams(34));
    EXPECT_EQ(CrossDockAllocation::boxesOf(receipt, "strawberry", 2, 7, available).cargo().weight, grams(67));
}

TEST(CrossDockAllocation, NothingAvailableGivesInvalidAllocation) {
    const Receipt receipt = Receipt::create(42, Settlement::OnSite, {pallet("strawberry", 3, grams(100))});
    EXPECT_FALSE(CrossDockAllocation::boxesOf(receipt, "strawberry", 1, 7, Cargo{}).isValid());
}

TEST(CrossDockAllocation, BoxesOfUnknownCropIsProgrammerError) {
    EXPECT_THROW((void)CrossDockAllocation::boxesOf(sampleReceipt(), "strawberry", 1, 7), std::invalid_argument);
}

TEST(CrossDockAllocation, DoesNotChangeSupplierBalance) {
    const auto a = CrossDockAllocation::wholeWeighing(sampleReceipt(), 0, 7);
    EXPECT_TRUE(a.balanceEffect().isZero());
    EXPECT_EQ(a.kind(), TransactionKind::Allocation);
    EXPECT_EQ(a.syncStatus(), SyncStatus::Pending);
}

TEST(CrossDockAllocation, ValidatesOperatorInput) {
    EXPECT_FALSE(CrossDockAllocation::boxesOf(sampleReceipt(), "raspberry", 0, 7).isValid());
    EXPECT_FALSE(CrossDockAllocation::boxesOf(sampleReceipt(), "raspberry", 10, 0).isValid()); // no truck chosen
}

TEST(CrossDockAllocation, DescribeMentionsTruckAndCargo) {
    const auto text = CrossDockAllocation::wholeWeighing(sampleReceipt(), 2, 7).describe();
    EXPECT_NE(text.find("blueberry"), std::string::npos);
    EXPECT_NE(text.find("40 ящ."), std::string::npos);
    EXPECT_NE(text.find("фура 7"), std::string::npos);
}
