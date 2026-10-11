#include "wms/sqlite_session_store.h"

#include "wms/sqlite_journal_store.h"

#include <gtest/gtest.h>

#include <chrono>

using namespace wms;
using namespace std::chrono;
using namespace std::chrono_literals;

namespace {

constexpr year_month_day kDate = October / 11 / 2026;
const TimePoint kMidnight = TimePoint{sys_days{kDate}};
constexpr OperatorId kOksana = 1;
constexpr OperatorId kPetro = 2;

Weighing raspberryPallet() {
    return {
        .cropCode = "raspberry",
        .gross = kilograms(120),
        .boxCount = 40,
        .tarePerBox = grams(350),
        .palletTare = kilograms(25),
        .weightSource = WeightSource::Manual,
    };
}

// An open session with something of every kind in it.
WorkDay busyDay() {
    WorkDay day{kDate, kMidnight + 12h, kOksana};
    day.setPrice("raspberry", hryvnias(85), kMidnight + 12h);
    day.setPrice("raspberry", kopecks(9050), kMidnight + 15h);
    day.addTruck("BI1234AA", {.boxes = 1'200, .weight = kilograms(2'000)}, {.make = "DAF XF", .note = "Іван, на Київ"});
    const TruckId leaving = day.addTruck("BI5678BB", {.boxes = 0, .weight = kilograms(5'000)}).id();
    day.departTruck(leaving);
    const Uuid receipt = day.openReceipt(42).id();
    day.addWeighing(receipt, raspberryPallet(), kMidnight + 13h);
    day.addWeighing(receipt, raspberryPallet(), kMidnight + 16h);
    day.cancelWeighing(receipt, 0, Journal{});
    (void)day.openReceipt(43); // still empty
    return day;
}

} // namespace

TEST(SqliteSessionStore, NewDatabaseHasNoSession) {
    sqlite::Database db{":memory:"};
    const SqliteSessionStore store{db};
    EXPECT_FALSE(store.current().has_value());
    EXPECT_FALSE(store.lastClosed().has_value());
}

TEST(SqliteSessionStore, OpenSessionComesBackAsItWas) {
    sqlite::Database db{":memory:"};
    SqliteSessionStore store{db};
    const WorkDay day = busyDay();
    store.save(day);

    const auto loaded = store.current();
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->id(), day.id());
    EXPECT_EQ(loaded->date(), kDate);
    EXPECT_EQ(loaded->openedAt(), day.openedAt());
    EXPECT_EQ(loaded->openedBy(), kOksana);
    EXPECT_TRUE(loaded->isOpen());

    // Price history, not just the current price: earlier weighings kept the old one.
    ASSERT_EQ(loaded->prices().history().size(), 2U);
    EXPECT_EQ(loaded->prices().priceAt("raspberry", kMidnight + 13h), hryvnias(85));
    EXPECT_EQ(loaded->prices().current("raspberry"), kopecks(9050));

    ASSERT_EQ(loaded->trucks().size(), 2U);
    const Truck& first = loaded->trucks()[0];
    EXPECT_EQ(first.id(), day.trucks()[0].id());
    EXPECT_EQ(first.plate(), "BI1234AA");
    EXPECT_EQ(first.capacity(), day.trucks()[0].capacity());
    EXPECT_EQ(first.details(), day.trucks()[0].details());
    EXPECT_EQ(first.status(), TruckStatus::Loading);
    EXPECT_EQ(loaded->trucks()[1].status(), TruckStatus::Departed);

    ASSERT_EQ(loaded->openReceipts().size(), 2U);
    const OpenReceipt& receipt = loaded->openReceipts()[0];
    const OpenReceipt& original = day.openReceipts()[0];
    EXPECT_EQ(receipt.id(), original.id());
    EXPECT_EQ(receipt.openedAt(), original.openedAt());
    EXPECT_EQ(receipt.supplierId(), 42);
    ASSERT_EQ(receipt.weighings().size(), 2U);
    EXPECT_TRUE(receipt.weighings()[0].cancelled);
    EXPECT_EQ(receipt.weighings()[1].pricePerKg, kopecks(9050));
    EXPECT_EQ(receipt.weighings()[1].weightSource, WeightSource::Manual);
    EXPECT_TRUE(loaded->openReceipts()[1].weighings().empty());
}

TEST(SqliteSessionStore, SavingAgainReplacesTheOldState) {
    sqlite::Database db{":memory:"};
    SqliteSessionStore store{db};
    WorkDay day = busyDay();
    store.save(day);

    const Uuid emptyReceipt = day.openReceipts()[1].id();
    day.addWeighing(emptyReceipt, raspberryPallet(), kMidnight + 17h);
    (void)day.closeReceipt(emptyReceipt, Settlement::OnSite);
    day.departTruck(day.trucks()[0].id());
    store.save(day);

    const auto loaded = store.current();
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->openReceipts().size(), 1U); // the closed one went to the journal
    EXPECT_EQ(loaded->trucks().size(), 2U);
    EXPECT_EQ(loaded->trucks()[0].status(), TruckStatus::Departed);
    EXPECT_EQ(loaded->prices().history().size(), 2U);
}

TEST(SqliteSessionStore, ClosedSessionKeepsWhoClosedItAndAmendments) {
    sqlite::Database db{":memory:"};
    SqliteSessionStore store{db};
    WorkDay day{kDate, kMidnight + 12h, kOksana};
    day.close(kMidnight + 24h + 3h, kPetro);
    day.setPrice("strawberry", hryvnias(60), kMidnight + 12h,
                 Amendment{.by = kOksana, .at = kMidnight + 30h, .reason = "забули ціну"});
    store.save(day);

    EXPECT_FALSE(store.current().has_value());
    const auto loaded = store.lastClosed();
    ASSERT_TRUE(loaded.has_value());
    EXPECT_FALSE(loaded->isOpen());
    EXPECT_EQ(loaded->closedAt(), kMidnight + 24h + 3h);
    EXPECT_EQ(loaded->closedBy(), kPetro);
    ASSERT_EQ(loaded->amendments().size(), 1U);
    const AmendmentRecord& record = loaded->amendments()[0];
    EXPECT_EQ(record.amendment.by, kOksana);
    EXPECT_EQ(record.amendment.at, kMidnight + 30h);
    EXPECT_EQ(record.amendment.reason, "забули ціну");
    EXPECT_EQ(record.change, day.amendments()[0].change);
}

TEST(SqliteSessionStore, OpenAndClosedSessionsAreFoundApart) {
    sqlite::Database db{":memory:"};
    SqliteSessionStore store{db};
    WorkDay yesterday{kDate, kMidnight + 12h, kOksana};
    const TruckId carried = yesterday.addTruck("BI5678BB", {.boxes = 100}).id();
    yesterday.carryOverTruck(carried);
    yesterday.close(kMidnight + 20h, kOksana);
    store.save(yesterday);

    // The carried-over truck is in both sessions under the same id.
    WorkDay today{year_month_day{sys_days{kDate} + days{1}}, kMidnight + 36h, kPetro};
    today.adoptTruck(yesterday.trucks()[0]);
    store.save(today);

    const auto open = store.current();
    ASSERT_TRUE(open.has_value());
    EXPECT_EQ(open->id(), today.id());
    ASSERT_EQ(open->trucks().size(), 1U);
    EXPECT_EQ(open->trucks()[0].id(), carried);
    EXPECT_EQ(open->trucks()[0].status(), TruckStatus::Loading);

    // While today is open, "repeat a truck" still needs yesterday's.
    const auto closed = store.lastClosed();
    ASSERT_TRUE(closed.has_value());
    EXPECT_EQ(closed->id(), yesterday.id());
    EXPECT_EQ(closed->trucks()[0].status(), TruckStatus::CarriedOver);

    today.departTruck(carried);
    today.close(kMidnight + 44h, kPetro);
    store.save(today);
    EXPECT_FALSE(store.current().has_value());
    EXPECT_EQ(store.lastClosed()->id(), today.id());

    // Closed sessions stay in the database as the archive.
    sqlite::Statement count = db.prepare("SELECT count(*) FROM session");
    ASSERT_TRUE(count.step());
    EXPECT_EQ(count.columnInt(0), 2);
}

TEST(SqliteSessionStore, SharesTheDatabaseWithTheJournal) {
    // Both stores bring the schema up to date; the second finds nothing to do.
    sqlite::Database db{":memory:"};
    const SqliteJournalStore journal{db};
    SqliteSessionStore sessions{db};
    sessions.save(busyDay());
    EXPECT_TRUE(sessions.current().has_value());
    EXPECT_EQ(journal.load().size(), 0U);
}
