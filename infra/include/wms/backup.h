#pragma once

#include "wms/sqlite.h"
#include "wms/transaction.h"

#include <cstddef>
#include <filesystem>

namespace wms {

// Copies the database into `folder` (created if missing) as
// terminal-YYYYMMDD-HHMMSS.mmmZ.db, UTC, so names sort by time, then deletes
// the oldest such copies beyond `keep`; other files in the folder stay.
// Returns the copy's path. Throws sqlite::Error or std::filesystem_error on
// failure, std::invalid_argument for keep == 0 (that would delete the copy).
std::filesystem::path backupDatabase(sqlite::Database& db, const std::filesystem::path& folder, TimePoint at,
                                     std::size_t keep);

} // namespace wms
