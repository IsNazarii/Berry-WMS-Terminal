#pragma once

#include "wms/journal_store.h"
#include "wms/sqlite.h"

namespace wms {

// The journal in SQLite: one table per record type, receipt lines in their own
// table. Each save() is one transaction, so a receipt is never half-written.
class SqliteJournalStore final : public JournalStore {
public:
    // Creates the tables in a new database. Throws std::runtime_error for a
    // database written by a newer program version: its schema is unknown here.
    // The database must outlive the store.
    explicit SqliteJournalStore(sqlite::Database& db);

    bool save(const Receipt& receipt) override;
    bool save(const Payment& payment) override;
    bool save(const Allocation& allocation) override;
    bool save(const StockShipment& shipment) override;

    [[nodiscard]] Journal load() const override;

private:
    sqlite::Database& db_;
};

} // namespace wms
