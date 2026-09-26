#pragma once

#include "wms/allocation.h"
#include "wms/payment.h"
#include "wms/receipt.h"
#include "wms/supplier.h"

#include <cstddef>
#include <set>
#include <vector>

namespace wms {

// Append-only log of every fact the terminal knows about. Balances and truck
// loads are never stored: they are derived from the log on request, so records
// from another terminal can be merged in any order without losing data.
class Journal {
public:
    // Each record type has its own overload and storage, so queries never need
    // to guess the concrete type behind a Transaction&.
    // Returns false when a record with the same id is already in the journal:
    // sync may deliver a record twice, and the UUID makes that harmless.
    bool add(Receipt receipt);
    bool add(Payment payment);
    bool add(CrossDockAllocation allocation);

    [[nodiscard]] std::size_t size() const noexcept { return ids_.size(); }

    // What we owe the supplier: positive = we owe them, negative = they were overpaid.
    [[nodiscard]] Money balanceOf(SupplierId supplierId) const noexcept;
    // Everything allocated to the truck; compare with Truck::capacity() to spot overload.
    [[nodiscard]] Cargo loadOf(TruckId truckId) const noexcept;

private:
    // Remembers the id; false when it was already there.
    bool registerId(const Uuid& id);

    std::set<Uuid> ids_;
    std::vector<Receipt> receipts_;
    std::vector<Payment> payments_;
    std::vector<CrossDockAllocation> allocations_;
};

} // namespace wms
