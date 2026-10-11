#pragma once

#include "wms/sqlite.h"

namespace wms {

// Brings the terminal's database up to the schema this program expects.
// The version lives in PRAGMA user_version: 0 is a fresh file, each migration
// step raises it by one, so a terminal updated after weeks offline still gets
// every step in order. Throws std::runtime_error for a database from a newer
// program version: writing to it could lose data that version relies on.
void migrateSchema(sqlite::Database& db);

} // namespace wms
