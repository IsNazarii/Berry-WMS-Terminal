#pragma once

#include "wms/allocation.h"
#include "wms/journal.h"
#include "wms/payment.h"
#include "wms/receipt.h"
#include "wms/shipment.h"

namespace wms {

// Where the journal lives between runs. The core only knows this interface:
// infra keeps the records in SQLite, and the rules never depend on how.
// Records are only ever added, like in the Journal itself.
class JournalStore {
public:
    virtual ~JournalStore() = default;

    // Writes the record at once, so a crash right after loses nothing.
    // Returns false when a record with the same id is already stored,
    // mirroring Journal::add(): a twice-delivered record is harmless.
    virtual bool save(const Receipt& receipt) = 0;
    virtual bool save(const Payment& payment) = 0;
    virtual bool save(const Allocation& allocation) = 0;
    virtual bool save(const StockShipment& shipment) = 0;

    // Every stored record, each type in the order it was created.
    [[nodiscard]] virtual Journal load() const = 0;

protected:
    JournalStore() = default;
    JournalStore(const JournalStore&) = default;
    JournalStore& operator=(const JournalStore&) = default;
};

} // namespace wms
