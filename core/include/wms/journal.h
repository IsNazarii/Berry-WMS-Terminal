#pragma once

#include "wms/allocation.h"
#include "wms/payment.h"
#include "wms/receipt.h"
#include "wms/supplier.h"

#include <cstddef>
#include <set>
#include <string>
#include <string_view>
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

    // Part of the crop in the receipt that no truck has taken yet.
    // Throws std::invalid_argument when the receipt is not in the journal.
    [[nodiscard]] Cargo unallocatedOf(const Uuid& receiptId, std::string_view cropCode) const;

    // Operator-facing check before an allocation is added: its own errors plus
    // the rules that need the rest of the journal. add() does not call this on
    // purpose: facts synced from another terminal are accepted as they are.
    [[nodiscard]] std::vector<std::string> validateAllocation(const CrossDockAllocation& allocation) const;

private:
    // Remembers the id; false when it was already there.
    bool registerId(const Uuid& id);
    [[nodiscard]] const Receipt* findReceipt(const Uuid& id) const noexcept;

    std::set<Uuid> ids_;
    std::vector<Receipt> receipts_;
    std::vector<Payment> payments_;
    std::vector<CrossDockAllocation> allocations_;
};

} // namespace wms
