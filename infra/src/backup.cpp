#include "wms/backup.h"

#include <algorithm>
#include <chrono>
#include <format>
#include <stdexcept>
#include <string>
#include <vector>

namespace wms {
namespace {

namespace fs = std::filesystem;

constexpr std::string_view kPrefix = "terminal-";
constexpr std::string_view kSuffix = "Z.db";

bool isBackupName(const std::string& name) {
    return name.starts_with(kPrefix) && name.ends_with(kSuffix);
}

void deleteOldest(const fs::path& folder, std::size_t keep) {
    std::vector<fs::path> copies;
    for (const auto& entry : fs::directory_iterator{folder}) {
        if (entry.is_regular_file() && isBackupName(entry.path().filename().string())) copies.push_back(entry.path());
    }
    if (copies.size() <= keep) return;
    // Names carry the time with fixed-width fields, so name order is time order.
    std::ranges::sort(copies);
    for (std::size_t i = 0; i < copies.size() - keep; ++i) fs::remove(copies[i]);
}

} // namespace

fs::path backupDatabase(sqlite::Database& db, const fs::path& folder, TimePoint at, std::size_t keep) {
    if (keep == 0) throw std::invalid_argument("A backup must keep at least the new copy");
    fs::create_directories(folder);
    const auto ms = std::chrono::floor<std::chrono::milliseconds>(at);
    const fs::path copy = folder / std::format("{}{:%Y%m%d-%H%M%S}{}", kPrefix, ms, kSuffix);
    // Two copies within one millisecond would be the same data anyway.
    if (!fs::exists(copy)) db.backupTo(copy.string());
    deleteOldest(folder, keep);
    return copy;
}

} // namespace wms
