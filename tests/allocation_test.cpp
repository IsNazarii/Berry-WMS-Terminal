#include "wms/allocation.h"

#include <gtest/gtest.h>

#include <stdexcept>
#include <utility>

using namespace wms;

namespace {

// Fixed truck ids, so a failing test prints the same values every run.
const TruckId kTruck = Uuid::generateV7(TimePoint{}, 1, 7);
const TruckId kOtherTruck = Uuid::generateV7(TimePoint{}, 1, 8);

Weighing pallet(std::string crop, int boxes, Weight net) {
    return {
        .cropCode = std::move(crop),
        .gross = net + grams(300) * boxes + kilograms(25),
        .boxCount = boxes,
        .tarePerBox = grams(300),
        .palletTare = kilograms(25),
        .pricePerKg = hryvnias(85),
    };
}

// Two raspberry pallets (80 + 33 boxes) and one blueberry pallet.
Receipt sampleReceipt() {
    return Receipt::create(42, Settlement::OnSite, {pallet("raspberry", 80, kilograms(160)), pallet("raspberry", 33, grams(66'000)),
                                pallet("blueberry", 40, kilograms(70))});
}

} // namespace

TEST(Allocation, WholeWeighingTakesExactCargo) {
    const Receipt receipt = sampleReceipt();
    const auto a = Allocation::wholeWeighing(receipt, 1, kTruck);

    EXPECT_EQ(a.receiptId(), receipt.id());
    EXPECT_EQ(a.destination(), Destination{kTruck});
    EXPECT_EQ(a.cropCode(), "raspberry");
    EXPECT_EQ(a.cargo(), (Cargo{.boxes = 33, .weight = grams(66'000)}));
    EXPECT_EQ(a.weighingIndex(), 1U);
    EXPECT_TRUE(a.isValid());
}

TEST(Allocation, WholeWeighingRejectsBadIndex) {
    EXPECT_THROW((void)Allocation::wholeWeighing(sampleReceipt(), 3, kTruck), std::out_of_range);
}

TEST(Allocation, BoxesOfCropGetProportionalWeight) {
    // Raspberry in the receipt: 113 boxes, 226 kg -> 2 kg per box.
    const auto a = Allocation::boxesOf(sampleReceipt(), "raspberry", 50, kTruck);
    EXPECT_EQ(a.cargo(), (Cargo{.boxes = 50, .weight = kilograms(100)}));
    EXPECT_FALSE(a.weighingIndex().has_value());
}

TEST(Allocation, ProportionalWeightIsRoundedToGrams) {
    // 100 g over 3 boxes -> 33.3 g per box.
    const Receipt receipt = Receipt::create(42, Settlement::OnSite, {pallet("strawberry", 3, grams(100))});
    EXPECT_EQ(Allocation::boxesOf(receipt, "strawberry", 1, kTruck).cargo().weight, grams(33));
    EXPECT_EQ(Allocation::boxesOf(receipt, "strawberry", 2, kTruck).cargo().weight, grams(67));
}

TEST(Allocation, BoxesOfSplitWhatIsStillAvailable) {
    // 100 g over 3 boxes, one box already gone (33 g): the other two share the remaining 67 g.
    const Receipt receipt = Receipt::create(42, Settlement::OnSite, {pallet("strawberry", 3, grams(100))});
    const Cargo available{.boxes = 2, .weight = grams(67)};
    EXPECT_EQ(Allocation::boxesOf(receipt, "strawberry", 1, kTruck, available).cargo().weight, grams(34));
    EXPECT_EQ(Allocation::boxesOf(receipt, "strawberry", 2, kTruck, available).cargo().weight, grams(67));
}

TEST(Allocation, NothingAvailableGivesInvalidAllocation) {
    const Receipt receipt = Receipt::create(42, Settlement::OnSite, {pallet("strawberry", 3, grams(100))});
    EXPECT_FALSE(Allocation::boxesOf(receipt, "strawberry", 1, kTruck, Cargo{}).isValid());
}

TEST(Allocation, BoxesOfUnknownCropIsProgrammerError) {
    EXPECT_THROW((void)Allocation::boxesOf(sampleReceipt(), "strawberry", 1, kTruck), std::invalid_argument);
}

TEST(Allocation, DoesNotChangeSupplierBalance) {
    const auto a = Allocation::wholeWeighing(sampleReceipt(), 0, kTruck);
    EXPECT_TRUE(a.balanceEffect().isZero());
    EXPECT_EQ(a.kind(), TransactionKind::Allocation);
    EXPECT_EQ(a.syncStatus(), SyncStatus::Pending);
}

TEST(Allocation, ValidatesOperatorInput) {
    EXPECT_FALSE(Allocation::boxesOf(sampleReceipt(), "raspberry", 0, kTruck).isValid());
    EXPECT_FALSE(Allocation::boxesOf(sampleReceipt(), "raspberry", 10, TruckId{}).isValid()); // no truck chosen
}

TEST(Allocation, DescribeMentionsTruckAndCargo) {
    const auto text = Allocation::wholeWeighing(sampleReceipt(), 2, kTruck).describe();
    EXPECT_NE(text.find("blueberry"), std::string::npos);
    EXPECT_NE(text.find("40 ящ."), std::string::npos);
    EXPECT_NE(text.find("фура " + kTruck.toString()), std::string::npos);
}

TEST(Allocation, CanGoToTheWarehouse) {
    const auto a = Allocation::wholeWeighing(sampleReceipt(), 2, ToWarehouse{});
    EXPECT_TRUE(a.isValid());
    EXPECT_TRUE(std::holds_alternative<ToWarehouse>(a.destination()));
    EXPECT_NE(a.describe().find("склад"), std::string::npos);
}

TEST(Allocation, TruckDestinationKeepsTheTruck) {
    const auto a = Allocation::boxesOf(sampleReceipt(), "raspberry", 10, kTruck);
    EXPECT_EQ(a.destination(), Destination{kTruck});
}
