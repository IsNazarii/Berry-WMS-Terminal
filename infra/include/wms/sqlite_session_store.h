#pragma once

#include "wms/session_store.h"
#include "wms/sqlite.h"

namespace wms {

// Sessions in SQLite: a row per session plus its lists (price changes,
// trucks, amendments, open receipts with their lines). save() rewrites the
// lists in one transaction, so a crash leaves either the old state or the new.
class SqliteSessionStore final : public SessionStore {
public:
    // Brings the schema up to date, like SqliteJournalStore; the two may share
    // one database. The database must outlive the store.
    explicit SqliteSessionStore(sqlite::Database& db);

    void save(const WorkDay& day) override;
    [[nodiscard]] std::optional<WorkDay> current() const override;
    [[nodiscard]] std::optional<WorkDay> lastClosed() const override;

private:
    sqlite::Database& db_;
};

} // namespace wms
