#include "wms/sqlite_repository.h"

#include "wms/sqlite_journal_store.h"

#include <gtest/gtest.h>

using namespace wms;

TEST(SqliteRepository, NewDatabaseHasNoReferenceData) {
    sqlite::Database db{":memory:"};
    EXPECT_TRUE(SqliteRepository<Supplier>{db}.all().empty());
    EXPECT_TRUE(SqliteRepository<Operator>{db}.all().empty());
}

TEST(SqliteRepository, SuppliersComeBackInIdOrder) {
    sqlite::Database db{":memory:"};
    SqliteRepository<Supplier> suppliers{db};
    suppliers.save({77, "ФГ «Ягідний край»"});
    suppliers.save({42, "Шевченко Тарас", "+380671112233"});

    const auto loaded = suppliers.all();
    ASSERT_EQ(loaded.size(), 2U);
    EXPECT_EQ(loaded[0].id(), 42);
    EXPECT_EQ(loaded[0].fullName(), "Шевченко Тарас");
    EXPECT_EQ(loaded[0].phone(), "+380671112233");
    EXPECT_EQ(loaded[1].id(), 77);
    EXPECT_EQ(loaded[1].phone(), "");
}

// The server will send changed records again: same id = the newer version.
TEST(SqliteRepository, SavingTheSameIdReplacesTheRecord) {
    sqlite::Database db{":memory:"};
    SqliteRepository<Supplier> suppliers{db};
    suppliers.save({42, "Шевченко Тарас"});
    suppliers.save({42, "Шевченко Тарас Г.", "+380671112233"});

    const auto loaded = suppliers.all();
    ASSERT_EQ(loaded.size(), 1U);
    EXPECT_EQ(loaded[0].fullName(), "Шевченко Тарас Г.");
    EXPECT_EQ(loaded[0].phone(), "+380671112233");
}

TEST(SqliteRepository, OperatorKeepsThePin) {
    sqlite::Database db{":memory:"};
    SqliteRepository<Operator> operators{db};
    operators.save({1, "Оксана", "4821"});

    const auto loaded = operators.all();
    ASSERT_EQ(loaded.size(), 1U);
    EXPECT_EQ(loaded[0].name(), "Оксана");
    EXPECT_TRUE(loaded[0].checkPin("4821"));
}

// Reference data shares the database file with the journal and sessions.
TEST(SqliteRepository, SharesTheDatabaseWithOtherStores) {
    sqlite::Database db{":memory:"};
    SqliteJournalStore journal{db};
    SqliteRepository<Supplier> suppliers{db};
    suppliers.save({42, "Шевченко Тарас"});
    EXPECT_EQ(SqliteRepository<Supplier>{db}.all().size(), 1U);
}

// Crops have no number; they come back in the order they were added, which
// is the order the operator is used to seeing them in.
TEST(SqliteRepository, CropsComeBackInTheOrderAdded) {
    sqlite::Database db{":memory:"};
    SqliteRepository<Crop> crops{db};
    crops.save({"raspberry", "Малина"});
    crops.save({"strawberry", "Полуниця"});
    crops.save({"агрус", "Агрус"});
    crops.save({"raspberry", "Малина ремонтантна"});

    const auto loaded = crops.all();
    ASSERT_EQ(loaded.size(), 3U);
    EXPECT_EQ(loaded[0].code(), "raspberry");
    EXPECT_EQ(loaded[0].name(), "Малина ремонтантна");
    EXPECT_EQ(loaded[1].code(), "strawberry");
    EXPECT_EQ(loaded[2].code(), "агрус");
}

TEST(SqliteRepository, CropKeepsTheArchivedMark) {
    sqlite::Database db{":memory:"};
    SqliteRepository<Crop> crops{db};
    crops.save({"raspberry", "Малина", true});
    crops.save({"strawberry", "Полуниця"});

    const auto loaded = crops.all();
    ASSERT_EQ(loaded.size(), 2U);
    EXPECT_TRUE(loaded[0].archived());
    EXPECT_FALSE(loaded[1].archived());
}

// A crop added by mistake and never used leaves no trace.
TEST(SqliteRepository, RemoveDeletesTheRecord) {
    sqlite::Database db{":memory:"};
    SqliteRepository<Crop> crops{db};
    crops.save({"raspberry", "Малина"});
    crops.save({"малинаа", "Малинаа"});
    crops.remove({"малинаа", "Малинаа"});

    const auto loaded = crops.all();
    ASSERT_EQ(loaded.size(), 1U);
    EXPECT_EQ(loaded[0].code(), "raspberry");

    SqliteRepository<Supplier> suppliers{db};
    suppliers.save({42, "Шевченко Тарас"});
    suppliers.remove({42, "Шевченко Тарас"});
    EXPECT_TRUE(suppliers.all().empty());
}
