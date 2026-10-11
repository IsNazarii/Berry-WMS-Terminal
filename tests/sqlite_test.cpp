#include "wms/sqlite.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <optional>

using namespace wms::sqlite;

namespace {

Database memoryWithTable() {
    Database db{":memory:"};
    db.execute("CREATE TABLE item (id INTEGER PRIMARY KEY, name TEXT NOT NULL, grams INTEGER)");
    return db;
}

} // namespace

TEST(SqliteTest, BindsAndReadsBackValues) {
    Database db = memoryWithTable();
    Statement insert = db.prepare("INSERT INTO item (name, grams) VALUES (?1, ?2)");
    insert.bind(1, "малина").bind(2, std::int64_t{185'400});
    EXPECT_FALSE(insert.step());

    Statement select = db.prepare("SELECT id, name, grams FROM item");
    ASSERT_TRUE(select.step());
    EXPECT_EQ(select.columnInt(0), db.lastInsertRowId());
    EXPECT_EQ(select.columnText(1), "малина");
    EXPECT_EQ(select.columnInt(2), 185'400);
    EXPECT_FALSE(select.step());
}

TEST(SqliteTest, NullIsToldApartFromZero) {
    Database db = memoryWithTable();
    db.prepare("INSERT INTO item (name, grams) VALUES ('a', NULL)").step();
    db.prepare("INSERT INTO item (name, grams) VALUES ('b', 0)").step();

    Statement select = db.prepare("SELECT grams FROM item ORDER BY id");
    ASSERT_TRUE(select.step());
    EXPECT_TRUE(select.isNull(0));
    ASSERT_TRUE(select.step());
    EXPECT_FALSE(select.isNull(0));
}

TEST(SqliteTest, OptionalBindsNullWhenEmpty) {
    Database db = memoryWithTable();
    Statement insert = db.prepare("INSERT INTO item (name, grams) VALUES ('a', ?1)");
    insert.bind(1, std::optional<std::int64_t>{});
    insert.step();

    Statement select = db.prepare("SELECT grams FROM item");
    ASSERT_TRUE(select.step());
    EXPECT_TRUE(select.isNull(0));
}

TEST(SqliteTest, StatementIsReusedAfterReset) {
    Database db = memoryWithTable();
    Statement insert = db.prepare("INSERT INTO item (name) VALUES (?1)");
    insert.bind(1, "a").step();
    insert.reset();
    insert.bind(1, "b").step();

    Statement count = db.prepare("SELECT count(*) FROM item");
    ASSERT_TRUE(count.step());
    EXPECT_EQ(count.columnInt(0), 2);
}

TEST(SqliteTest, BadSqlThrowsWithMessage) {
    Database db{":memory:"};
    try {
        (void)db.prepare("SELEC 1");
        FAIL() << "expected sqlite::Error";
    } catch (const Error& e) {
        EXPECT_NE(std::string{e.what()}.find("syntax error"), std::string::npos);
    }
}

TEST(SqliteTest, ConstraintViolationThrowsOnStep) {
    Database db = memoryWithTable();
    Statement insert = db.prepare("INSERT INTO item (name) VALUES (NULL)");
    EXPECT_THROW(insert.step(), Error);
}

TEST(SqliteTest, ForeignKeysAreEnforced) {
    Database db{":memory:"};
    db.execute("CREATE TABLE parent (id INTEGER PRIMARY KEY);"
               "CREATE TABLE child (parent_id INTEGER NOT NULL REFERENCES parent (id))");
    EXPECT_THROW(db.execute("INSERT INTO child VALUES (42)"), Error);
}

TEST(SqliteTest, TransactionRollsBackWithoutCommit) {
    Database db = memoryWithTable();
    {
        ScopedTransaction transaction{db};
        db.execute("INSERT INTO item (name) VALUES ('lost')");
    }
    {
        ScopedTransaction transaction{db};
        db.execute("INSERT INTO item (name) VALUES ('kept')");
        transaction.commit();
    }

    Statement select = db.prepare("SELECT name FROM item");
    ASSERT_TRUE(select.step());
    EXPECT_EQ(select.columnText(0), "kept");
    EXPECT_FALSE(select.step());
}

TEST(SqliteTest, FileDatabaseUsesWriteAheadLog) {
    const auto path = std::filesystem::temp_directory_path() / "wms_sqlite_test.db";
    std::filesystem::remove(path);
    {
        Database db{path.string()};
        Statement mode = db.prepare("PRAGMA journal_mode");
        ASSERT_TRUE(mode.step());
        EXPECT_EQ(mode.columnText(0), "wal");
    }
    std::filesystem::remove(path);
    std::filesystem::remove(path.string() + "-wal");
    std::filesystem::remove(path.string() + "-shm");
}
