#include "wms/journal.h"
#include "wms/session_summary.h"
#include "wms/workday.h"

#include <gtest/gtest.h>

#include <chrono>

using namespace wms;
using namespace std::chrono;
using namespace std::chrono_literals;

namespace {

constexpr year_month_day kDate = October / 28 / 2026;
const TimePoint kMidnight = TimePoint{sys_days{kDate}};
constexpr OperatorId kOksana = 1;

Weighing pallet(const char* crop, int boxes, Weight net) {
    return {
        .cropCode = crop,
        .gross = net + grams(300) * boxes,
        .boxCount = boxes,
        .tarePerBox = grams(300),
    };
}

WorkDay openDay() {
    WorkDay day{kDate, kMidnight + 12h, kOksana};
    day.setPrice("raspberry", hryvnias(85), kMidnight + 12h);
    day.setPrice("strawberry", hryvnias(60), kMidnight + 12h);
    return day;
}

// Weighs a receipt of the day and pays it on the spot.
Receipt paidReceipt(WorkDay& day, Journal& journal, SupplierId supplier, Weighing w, PaymentMethod method) {
    const Uuid id = day.openReceipt(supplier).id();
    day.addWeighing(id, std::move(w), kMidnight + 13h);
    const std::string crop = day.findReceipt(id)->weighings()[0].cropCode;
    PaidReceipt paid = day.payReceipt(id, {{.cropCode = crop, .pricePerKg = hryvnias(85)}}, method, kOksana,
                                      kMidnight + 14h);
    journal.add(paid.receipt);
    journal.add(paid.payment);
    return paid.receipt;
}

} // namespace

TEST(SessionSummary, NewSessionHasNothing) {
    const SessionSummary summary{openDay(), Journal{}};
    EXPECT_EQ(summary.received().boxes, 0);
    EXPECT_TRUE(summary.byCrop().empty());
    EXPECT_EQ(summary.closedReceipts(), 0);
    EXPECT_EQ(summary.openReceipts(), 0);
    EXPECT_TRUE(summary.paid().isZero());
}

TEST(SessionSummary, CountsClosedAndOpenReceiptsOfThisSessionOnly) {
    WorkDay day = openDay();
    Journal journal;
    paidReceipt(day, journal, 42, pallet("raspberry", 10, kilograms(20)), PaymentMethod::Cash);
    // Still unloading: the berries are here already, so they count.
    const Uuid open = day.openReceipt(43).id();
    day.addWeighing(open, pallet("strawberry", 5, kilograms(9)), kMidnight + 15h);
    day.addWeighing(open, pallet("strawberry", 4, kilograms(7)), kMidnight + 15h);
    (void)day.cancelWeighing(open, 1, journal); // a mistake does not count
    // Another day's receipt and one from no session.
    journal.add(Receipt{Uuid::generateV7(), kMidnight, 44, Settlement::HandedOver,
                        {pallet("raspberry", 50, kilograms(100))}, SyncStatus::Pending, Uuid::generateV7()});
    journal.add(Receipt::create(45, Settlement::HandedOver, {pallet("raspberry", 50, kilograms(100))}));

    const SessionSummary summary{day, journal};
    EXPECT_EQ(summary.closedReceipts(), 1);
    EXPECT_EQ(summary.openReceipts(), 1);
    EXPECT_EQ(summary.received().boxes, 15);
    EXPECT_EQ(summary.received().weight, kilograms(29));
}

TEST(SessionSummary, CropsBiggestFirst) {
    WorkDay day = openDay();
    Journal journal;
    paidReceipt(day, journal, 42, pallet("strawberry", 5, kilograms(9)), PaymentMethod::Cash);
    paidReceipt(day, journal, 43, pallet("raspberry", 10, kilograms(20)), PaymentMethod::Cash);
    paidReceipt(day, journal, 44, pallet("strawberry", 1, kilograms(2)), PaymentMethod::Cash);

    const SessionSummary summary{day, journal};
    ASSERT_EQ(summary.byCrop().size(), 2U);
    EXPECT_EQ(summary.byCrop()[0].cropCode, "raspberry");
    EXPECT_EQ(summary.byCrop()[1].cropCode, "strawberry");
    EXPECT_EQ(summary.byCrop()[1].cargo.weight, kilograms(11));
}

TEST(SessionSummary, SaysWhereTheBoxesAre) {
    WorkDay day = openDay();
    Journal journal;
    const TruckId truck = day.addTruck("BI1234AA", 1'000).id();
    const Receipt toTruck = paidReceipt(day, journal, 42, pallet("raspberry", 10, kilograms(20)), PaymentMethod::Cash);
    const Receipt toStock = paidReceipt(day, journal, 43, pallet("raspberry", 6, kilograms(12)), PaymentMethod::Cash);
    paidReceipt(day, journal, 44, pallet("raspberry", 4, kilograms(8)), PaymentMethod::Cash); // waits
    journal.add(Allocation::wholeWeighing(toTruck, 0, truck));
    journal.add(Allocation::wholeWeighing(toStock, 0, ToWarehouse{}));

    const SessionSummary summary{day, journal};
    EXPECT_EQ(summary.inTrucks().boxes, 10);
    EXPECT_EQ(summary.inWarehouse().boxes, 6);
    EXPECT_EQ(summary.waiting().boxes, 4);
    EXPECT_EQ(summary.waiting().weight, kilograms(8));
}

TEST(SessionSummary, PaidSplitByMethodOfThisSessionOnly) {
    WorkDay day = openDay();
    Journal journal;
    paidReceipt(day, journal, 42, pallet("raspberry", 10, kilograms(20)), PaymentMethod::Cash);        // 1 700
    paidReceipt(day, journal, 43, pallet("raspberry", 5, kilograms(10)), PaymentMethod::BankTransfer); // 850
    journal.add(Payment::create(46, hryvnias(500), PaymentMethod::Cash, kOksana, day.id())); // an old debt, paid today
    journal.add(Payment::create(46, hryvnias(300), PaymentMethod::Cash, kOksana));          // no session open

    const SessionSummary summary{day, journal};
    EXPECT_EQ(summary.paidCash(), hryvnias(1'700 + 500));
    EXPECT_EQ(summary.paidTransfer(), hryvnias(850));
    EXPECT_EQ(summary.paid(), hryvnias(3'050));
}
