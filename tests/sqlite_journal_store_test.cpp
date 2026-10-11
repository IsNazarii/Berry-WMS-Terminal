#include "wms/sqlite_journal_store.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <string>
#include <utility>

using namespace wms;

namespace {

const TruckId kTruck = Uuid::generateV7(TimePoint{}, 1, 7);
// Not a whole number of microseconds: the store must keep the exact instant.
const TimePoint kAt = TimePoint{} + std::chrono::duration_cast<Clock::duration>(
                                         std::chrono::nanoseconds{1'790'000'000'123'456'789});

Weighing pallet(std::string crop, int boxes, Weight gross) {
    return {
        .cropCode = std::move(crop),
        .gross = gross,
        .boxCount = boxes,
        .tarePerBox = grams(300),
        .palletTare = kilograms(18),
        .pricePerKg = kopecks(8550),
    };
}

Receipt sampleReceipt() {
    Weighing manual = pallet("strawberry", 40, kilograms(110));
    manual.priceSource = PriceSource::Manual;
    manual.weightSource = WeightSource::Manual;
    manual.cancelled = true;
    return Receipt{Uuid::generateV7(kAt, 1, 1), kAt, 42, Settlement::OnSite,
                   {pallet("raspberry", 60, grams(185'400)), manual}};
}

} // namespace

TEST(SqliteJournalStore, NewDatabaseLoadsEmpty) {
    sqlite::Database db{":memory:"};
    const SqliteJournalStore store{db};
    EXPECT_EQ(store.load().size(), 0U);
}

TEST(SqliteJournalStore, ReceiptRoundTripsWithEveryLine) {
    sqlite::Database db{":memory:"};
    SqliteJournalStore store{db};
    const Receipt original = sampleReceipt();
    ASSERT_TRUE(store.save(original));

    const Journal journal = store.load();
    const Receipt* loaded = journal.findReceipt(original.id());
    ASSERT_NE(loaded, nullptr);
    EXPECT_EQ(loaded->createdAt(), kAt);
    EXPECT_EQ(loaded->supplierId(), 42);
    EXPECT_EQ(loaded->settlement(), Settlement::OnSite);
    EXPECT_EQ(loaded->syncStatus(), SyncStatus::Pending);
    ASSERT_EQ(loaded->weighings().size(), 2U);

    const Weighing& first = loaded->weighings()[0];
    EXPECT_EQ(first.cropCode, "raspberry");
    EXPECT_EQ(first.gross, grams(185'400));
    EXPECT_EQ(first.boxCount, 60);
    EXPECT_EQ(first.tarePerBox, grams(300));
    EXPECT_EQ(first.palletTare, kilograms(18));
    EXPECT_EQ(first.pricePerKg, kopecks(8550));
    EXPECT_EQ(first.priceSource, PriceSource::DayPrice);
    EXPECT_EQ(first.weightSource, WeightSource::Scale);
    EXPECT_FALSE(first.cancelled);

    const Weighing& second = loaded->weighings()[1];
    EXPECT_EQ(second.priceSource, PriceSource::Manual);
    EXPECT_EQ(second.weightSource, WeightSource::Manual);
    EXPECT_TRUE(second.cancelled);
    EXPECT_EQ(loaded->amount(), original.amount());
}

TEST(SqliteJournalStore, PaymentRoundTripsAndCountsInBalance) {
    sqlite::Database db{":memory:"};
    SqliteJournalStore store{db};
    const Receipt receipt = sampleReceipt();
    const Payment payment{Uuid::generateV7(kAt, 2, 2), kAt + std::chrono::minutes{5}, 42, hryvnias(1000),
                          PaymentMethod::BankTransfer, SyncStatus::Synced};
    store.save(receipt);
    store.save(payment);

    const Journal journal = store.load();
    EXPECT_EQ(journal.balanceOf(42), receipt.amount() - hryvnias(1000));
    const auto history = journal.historyOf(42);
    ASSERT_EQ(history.size(), 2U);
    const auto* loaded = dynamic_cast<const Payment*>(history[1]);
    ASSERT_NE(loaded, nullptr);
    EXPECT_EQ(loaded->id(), payment.id());
    EXPECT_EQ(loaded->createdAt(), payment.createdAt());
    EXPECT_EQ(loaded->method(), PaymentMethod::BankTransfer);
    EXPECT_EQ(loaded->syncStatus(), SyncStatus::Synced);
}

TEST(SqliteJournalStore, AllocationsKeepDestinationAndWeighingIndex) {
    sqlite::Database db{":memory:"};
    SqliteJournalStore store{db};
    const Receipt receipt = sampleReceipt();
    store.save(receipt);
    store.save(Allocation::wholeWeighing(receipt, 0, kTruck));
    // By boxes: no weighing index, so weighing 1 must stay free.
    store.save(Allocation::boxesOf(receipt, "raspberry", 5, ToWarehouse{}));

    const Journal journal = store.load();
    EXPECT_EQ(journal.loadOf(kTruck), receipt.weighings()[0].cargo());
    EXPECT_EQ(journal.stockOf("raspberry").boxes, 5);
    const auto destination = journal.destinationOf(receipt.id(), 0);
    ASSERT_TRUE(destination.has_value());
    EXPECT_EQ(*destination, Destination{kTruck});
    EXPECT_FALSE(journal.destinationOf(receipt.id(), 1).has_value());
}

TEST(SqliteJournalStore, WarehouseAllocationAndShipmentMakeStock) {
    sqlite::Database db{":memory:"};
    SqliteJournalStore store{db};
    const Receipt receipt = sampleReceipt();
    store.save(receipt);
    store.save(Allocation::wholeWeighing(receipt, 0, ToWarehouse{}));
    store.save(StockShipment{Uuid::generateV7(kAt, 4, 4), kAt, "raspberry", {.boxes = 10, .weight = kilograms(25)},
                             kTruck});

    const Journal journal = store.load();
    const Cargo put = receipt.weighings()[0].cargo();
    EXPECT_EQ(journal.stockOf("raspberry"), (Cargo{.boxes = put.boxes - 10, .weight = put.weight - kilograms(25)}));
    EXPECT_EQ(journal.loadOf(kTruck), (Cargo{.boxes = 10, .weight = kilograms(25)}));
}

TEST(SqliteJournalStore, SameIdIsSavedOnce) {
    sqlite::Database db{":memory:"};
    SqliteJournalStore store{db};
    const Receipt receipt = sampleReceipt();
    EXPECT_TRUE(store.save(receipt));
    EXPECT_FALSE(store.save(receipt));
    EXPECT_EQ(store.load().size(), 1U);
    ASSERT_EQ(store.load().findReceipt(receipt.id())->weighings().size(), 2U);
}

TEST(SqliteJournalStore, RecordsSurviveReopeningTheFile) {
    const auto path = std::filesystem::temp_directory_path() / "wms_journal_store_test.db";
    std::filesystem::remove(path);
    const Receipt receipt = sampleReceipt();
    {
        sqlite::Database db{path.string()};
        SqliteJournalStore store{db};
        store.save(receipt);
    }
    {
        sqlite::Database db{path.string()};
        const SqliteJournalStore store{db};
        EXPECT_NE(store.load().findReceipt(receipt.id()), nullptr);
    }
    std::filesystem::remove(path);
    std::filesystem::remove(path.string() + "-wal");
    std::filesystem::remove(path.string() + "-shm");
}

TEST(SqliteJournalStore, RefusesDatabaseFromNewerVersion) {
    sqlite::Database db{":memory:"};
    db.execute("PRAGMA user_version = 999");
    EXPECT_THROW(SqliteJournalStore{db}, std::runtime_error);
}
