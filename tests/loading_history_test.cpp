#include "wms/journal.h"
#include "wms/loading_history.h"
#include "wms/workday.h"

#include <gtest/gtest.h>

#include <chrono>
#include <algorithm>
#include <variant>

using namespace wms;
using namespace std::chrono;
using namespace std::chrono_literals;

namespace {

constexpr year_month_day kDate = November / 12 / 2026;
const TimePoint kMidnight = TimePoint{sys_days{kDate}};
constexpr OperatorId kOksana = 1;
constexpr SupplierId kShevchenko = 1001;
constexpr SupplierId kMelnyk = 1002;

Weighing pallet(const char* crop, int boxes, Weight net) {
    return {
        .cropCode = crop,
        .gross = net + grams(300) * boxes,
        .boxCount = boxes,
        .tarePerBox = grams(300),
    };
}

WorkDay openDay(year_month_day date = kDate) {
    return WorkDay{date, TimePoint{sys_days{date}} + 12h, kOksana};
}

// An open receipt of the day with the pallets weighed into it.
Uuid weigh(WorkDay& day, SupplierId supplier, std::initializer_list<Weighing> pallets) {
    const Uuid id = day.openReceipt(supplier).id();
    for (const Weighing& w : pallets) day.addWeighing(id, w, kMidnight + 13h);
    return id;
}

// Allocations at a fixed time, so the order of the history is the test's.
Allocation wholePallet(const WorkDay& day, const Uuid& receiptId, std::size_t index, Destination to, TimePoint at) {
    const Weighing& w = day.findReceipt(receiptId)->weighings().at(index);
    return Allocation{Uuid::generateV7(), at, receiptId, to, w.cropCode, w.cargo(), index};
}

Allocation boxes(const Uuid& receiptId, const char* crop, int count, Weight weight, Destination to, TimePoint at) {
    return Allocation{Uuid::generateV7(), at, receiptId, to, crop, {.boxes = count, .weight = weight}, std::nullopt};
}

} // namespace

TEST(LoadingHistory, NewDayHasNothing) {
    const LoadingHistory history{openDay(), Journal{}};
    EXPECT_TRUE(history.entries().empty());
    EXPECT_TRUE(history.intoTrucks().empty());
    EXPECT_TRUE(history.warehouse().empty());
}

TEST(LoadingHistory, PalletInTruckShowsSupplierAndItsNumber) {
    WorkDay day = openDay();
    const TruckId truck = day.addTruck("BI1111AA", 60).id();
    const Uuid receipt = weigh(day, kShevchenko, {pallet("raspberry", 10, kilograms(20)),
                                                  pallet("raspberry", 12, kilograms(24))});
    Journal journal;
    journal.add(wholePallet(day, receipt, 1, truck, kMidnight + 14h));

    const auto rows = LoadingHistory{day, journal}.intoTruck(truck);
    ASSERT_EQ(rows.size(), 1U);
    EXPECT_EQ(rows[0].supplierId, kShevchenko);
    EXPECT_EQ(rows[0].receiptId, receipt);
    EXPECT_EQ(rows[0].weighingIndex, 1U);
    EXPECT_EQ(rows[0].cropCode, "raspberry");
    EXPECT_EQ(rows[0].cargo.boxes, 12);
    EXPECT_EQ(rows[0].cargo.weight, kilograms(24));
    EXPECT_EQ(rows[0].at, kMidnight + 14h);
    EXPECT_FALSE(rows[0].fromWarehouse());
    EXPECT_FALSE(rows[0].reversed());
}

TEST(LoadingHistory, ClosedReceiptStillNamesItsSupplier) {
    WorkDay day = openDay();
    const TruckId truck = day.addTruck("BI1111AA", 60).id();
    const Uuid receipt = weigh(day, kMelnyk, {pallet("strawberry", 20, kilograms(60))});
    Journal journal;
    journal.add(wholePallet(day, receipt, 0, truck, kMidnight + 14h));
    journal.add(day.closeReceipt(receipt, Settlement::HandedOver));

    const auto rows = LoadingHistory{day, journal}.intoTruck(truck);
    ASSERT_EQ(rows.size(), 1U);
    EXPECT_EQ(rows[0].supplierId, kMelnyk);
}

TEST(LoadingHistory, BoxesCountedOutHaveNoPalletNumber) {
    WorkDay day = openDay();
    const TruckId truck = day.addTruck("BI1111AA", 60).id();
    const Uuid receipt = weigh(day, kShevchenko, {pallet("raspberry", 10, kilograms(20))});
    Journal journal;
    journal.add(boxes(receipt, "raspberry", 4, kilograms(8), truck, kMidnight + 14h));

    const auto rows = LoadingHistory{day, journal}.intoTruck(truck);
    ASSERT_EQ(rows.size(), 1U);
    EXPECT_EQ(rows[0].weighingIndex, std::nullopt);
    EXPECT_EQ(rows[0].cargo.boxes, 4);
}

TEST(LoadingHistory, ReversedPalletStaysListedWithItsTime) {
    WorkDay day = openDay();
    const TruckId truck = day.addTruck("BI1111AA", 60).id();
    const Uuid receipt = weigh(day, kShevchenko, {pallet("raspberry", 10, kilograms(20))});
    Journal journal;
    const Allocation allocation = wholePallet(day, receipt, 0, truck, kMidnight + 14h);
    journal.add(allocation);
    journal.add(AllocationReversal{Uuid::generateV7(), kMidnight + 15h, allocation.id()});

    const auto rows = LoadingHistory{day, journal}.intoTruck(truck);
    ASSERT_EQ(rows.size(), 1U);
    EXPECT_EQ(rows[0].reversedAt, kMidnight + 15h);
}

TEST(LoadingHistory, ShipmentFromWarehouseIsInTheTruckAndInTheWarehouse) {
    WorkDay day = openDay();
    const TruckId truck = day.addTruck("BI1111AA", 60).id();
    const Uuid receipt = weigh(day, kShevchenko, {pallet("raspberry", 10, kilograms(20))});
    Journal journal;
    journal.add(wholePallet(day, receipt, 0, ToWarehouse{}, kMidnight + 14h));
    journal.add(StockShipment{Uuid::generateV7(), kMidnight + 16h, "raspberry",
                              {.boxes = 6, .weight = kilograms(12)}, truck});

    const LoadingHistory history{day, journal};
    const auto inTruck = history.intoTruck(truck);
    ASSERT_EQ(inTruck.size(), 1U);
    EXPECT_TRUE(inTruck[0].fromWarehouse());
    EXPECT_EQ(inTruck[0].supplierId, std::nullopt);
    EXPECT_EQ(inTruck[0].cargo.boxes, 6);

    const auto warehouse = history.warehouse();
    ASSERT_EQ(warehouse.size(), 2U);
    EXPECT_TRUE(std::holds_alternative<ToWarehouse>(warehouse[0].to)); // in
    EXPECT_EQ(warehouse[1].to, Destination{truck});                    // out
}

TEST(LoadingHistory, WarehouseOfTheDayOnly) {
    // Yesterday's pallet is in the warehouse, but yesterday put it there.
    WorkDay yesterday = openDay(November / 11 / 2026);
    const Uuid old = weigh(yesterday, kMelnyk, {pallet("strawberry", 20, kilograms(60))});
    Journal journal;
    journal.add(wholePallet(yesterday, old, 0, ToWarehouse{}, kMidnight - 2h));
    journal.add(yesterday.closeReceipt(old, Settlement::HandedOver));

    WorkDay day = openDay();
    const Uuid today = weigh(day, kShevchenko, {pallet("raspberry", 10, kilograms(20))});
    journal.add(wholePallet(day, today, 0, ToWarehouse{}, kMidnight + 14h));

    const auto rows = LoadingHistory{day, journal}.warehouse();
    ASSERT_EQ(rows.size(), 1U);
    EXPECT_EQ(rows[0].receiptId, today);
}

TEST(LoadingHistory, TrucksOfOtherDaysAreNotListed) {
    WorkDay day = openDay();
    const TruckId ours = day.addTruck("BI1111AA", 60).id();
    const TruckId theirs = Uuid::generateV7();
    const Uuid receipt = weigh(day, kShevchenko, {pallet("raspberry", 10, kilograms(20)),
                                                  pallet("raspberry", 10, kilograms(20))});
    Journal journal;
    journal.add(wholePallet(day, receipt, 0, ours, kMidnight + 14h));
    journal.add(wholePallet(day, receipt, 1, theirs, kMidnight + 14h));

    const LoadingHistory history{day, journal};
    EXPECT_EQ(history.intoTrucks().size(), 1U);
    EXPECT_TRUE(history.intoTruck(theirs).empty());
}

TEST(LoadingHistory, CarriedOverTruckKeepsYesterdaysPallets) {
    WorkDay yesterday = openDay(November / 11 / 2026);
    const Truck truck = yesterday.addTruck("BI5678BB", 60);
    const Uuid old = weigh(yesterday, kMelnyk, {pallet("strawberry", 20, kilograms(60))});
    Journal journal;
    journal.add(wholePallet(yesterday, old, 0, truck.id(), kMidnight - 2h));
    journal.add(yesterday.closeReceipt(old, Settlement::HandedOver));
    yesterday.carryOverTruck(truck.id());

    WorkDay day = openDay();
    day.adoptTruck(yesterday.trucksForNextSession().at(0));
    const Uuid today = weigh(day, kShevchenko, {pallet("raspberry", 10, kilograms(20))});
    journal.add(wholePallet(day, today, 0, truck.id(), kMidnight + 14h));

    const auto rows = LoadingHistory{day, journal}.intoTruck(truck.id());
    ASSERT_EQ(rows.size(), 2U);
    EXPECT_EQ(rows[0].supplierId, kMelnyk);
    EXPECT_EQ(rows[1].supplierId, kShevchenko);
}

TEST(LoadingHistory, ArchivedDayFindsSupplierOfAReceiptStillOpenToday) {
    WorkDay yesterday = openDay(November / 11 / 2026);
    const Truck truck = yesterday.addTruck("BI5678BB", 60);
    yesterday.carryOverTruck(truck.id());
    WorkDay day = openDay();
    day.adoptTruck(yesterday.trucksForNextSession().at(0));
    const Uuid today = weigh(day, kShevchenko, {pallet("raspberry", 10, kilograms(20))});
    Journal journal;
    journal.add(wholePallet(day, today, 0, truck.id(), kMidnight + 14h));

    const auto rows = LoadingHistory{yesterday, journal, day.openReceipts()}.intoTruck(truck.id());
    ASSERT_EQ(rows.size(), 1U);
    EXPECT_EQ(rows[0].supplierId, kShevchenko);
}

TEST(LoadingHistory, OldestFirstAcrossRecordKinds) {
    WorkDay day = openDay();
    const TruckId truck = day.addTruck("BI1111AA", 60).id();
    const Uuid receipt = weigh(day, kShevchenko, {pallet("raspberry", 10, kilograms(20)),
                                                  pallet("raspberry", 10, kilograms(20))});
    Journal journal;
    journal.add(wholePallet(day, receipt, 0, ToWarehouse{}, kMidnight + 14h));
    journal.add(wholePallet(day, receipt, 1, truck, kMidnight + 16h));
    journal.add(StockShipment{Uuid::generateV7(), kMidnight + 15h, "raspberry",
                              {.boxes = 10, .weight = kilograms(20)}, truck});

    const auto& all = LoadingHistory{day, journal}.entries();
    ASSERT_EQ(all.size(), 3U);
    EXPECT_TRUE(std::ranges::is_sorted(all, {}, &LoadingEntry::at));
}
