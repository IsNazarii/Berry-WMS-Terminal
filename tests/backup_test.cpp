#include "wms/backup.h"

#include <gtest/gtest.h>

#include <chrono>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace wms;
using namespace std::chrono_literals;

namespace {

namespace fs = std::filesystem;

// 2026-10-16 13:30:05.123 UTC
const TimePoint kAt = TimePoint{std::chrono::sys_days{std::chrono::October / 16 / 2026}} + 13h + 30min + 5s + 123ms;

// A fresh, empty folder per test, removed afterwards.
class BackupTest : public ::testing::Test {
protected:
    void SetUp() override {
        folder_ = fs::temp_directory_path() / ("wms_backup_test_" + std::string{
                      ::testing::UnitTest::GetInstance()->current_test_info()->name()});
        fs::remove_all(folder_);
    }
    void TearDown() override { fs::remove_all(folder_); }

    [[nodiscard]] std::vector<std::string> files() const {
        std::vector<std::string> names;
        for (const auto& entry : fs::directory_iterator{folder_}) names.push_back(entry.path().filename().string());
        std::ranges::sort(names);
        return names;
    }

    fs::path folder_;
};

} // namespace

TEST_F(BackupTest, CopyHasTheDataAndATimeInItsName) {
    sqlite::Database db{":memory:"};
    db.execute("CREATE TABLE t (v TEXT); INSERT INTO t VALUES ('kept')");

    const fs::path copy = backupDatabase(db, folder_, kAt, 5);

    EXPECT_EQ(copy.filename(), "terminal-20261016-133005.123Z.db");
    sqlite::Database restored{copy.string()};
    sqlite::Statement select = restored.prepare("SELECT v FROM t");
    ASSERT_TRUE(select.step());
    EXPECT_EQ(select.columnText(0), "kept");
}

TEST_F(BackupTest, KeepsOnlyTheNewestCopies) {
    sqlite::Database db{":memory:"};
    for (int day = 0; day < 4; ++day) (void)backupDatabase(db, folder_, kAt + std::chrono::days{day}, 2);

    EXPECT_EQ(files(), (std::vector<std::string>{"terminal-20261018-133005.123Z.db",
                                                 "terminal-20261019-133005.123Z.db"}));
}

// The folder may hold other files (a copy the operator made by hand): they stay.
TEST_F(BackupTest, LeavesOtherFilesAlone) {
    sqlite::Database db{":memory:"};
    fs::create_directories(folder_);
    { std::ofstream{folder_ / "before-update.db"} << "x"; }
    (void)backupDatabase(db, folder_, kAt, 1);
    (void)backupDatabase(db, folder_, kAt + 1h, 1);

    EXPECT_EQ(files(), (std::vector<std::string>{"before-update.db", "terminal-20261016-143005.123Z.db"}));
}
