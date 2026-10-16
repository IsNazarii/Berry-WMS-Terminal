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
