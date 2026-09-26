#include "wms/shipment.h"

#include <gtest/gtest.h>

using namespace wms;

namespace {

const TruckId kTruck = Uuid::generateV7(TimePoint{}, 1, 7);

} // namespace

TEST(StockShipment, MovesStockIntoATruckWithoutTouchingBalances) {
    const auto s = StockShipment::create("raspberry", {.boxes = 20, .weight = kilograms(40)}, kTruck);
    EXPECT_TRUE(s.isValid());
    EXPECT_EQ(s.kind(), TransactionKind::Shipment);
    EXPECT_TRUE(s.balanceEffect().isZero()); // the supplier was paid when the berries came in
    EXPECT_EQ(s.truckId(), kTruck);
    EXPECT_NE(s.describe().find("склад"), std::string::npos);
}

TEST(StockShipment, ValidatesOperatorInput) {
    EXPECT_FALSE(StockShipment::create("", {.boxes = 1, .weight = kilograms(2)}, kTruck).isValid());
    EXPECT_FALSE(StockShipment::create("raspberry", {.boxes = 0, .weight = kilograms(2)}, kTruck).isValid());
    EXPECT_FALSE(StockShipment::create("raspberry", {.boxes = 1, .weight = {}}, kTruck).isValid());
    EXPECT_FALSE(StockShipment::create("raspberry", {.boxes = 1, .weight = kilograms(2)}, TruckId{}).isValid());
}
