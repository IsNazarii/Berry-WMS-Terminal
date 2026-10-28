#pragma once

#include "wms/allocation.h"
#include "wms/open_receipt.h"
#include "wms/payment.h"
#include "wms/receipt.h"
#include "wms/shipment.h"
#include "wms/supplier.h"

#include <cstddef>
#include <optional>
#include <ranges>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace wms {

// Where the berries of one receipt are now; the rest still waits for allocation.
struct Placement {
    Cargo inTrucks;
    Cargo inWarehouse;
};

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
    bool add(Allocation allocation);
    bool add(AllocationReversal reversal);
    bool add(StockShipment shipment);

    [[nodiscard]] std::size_t size() const noexcept { return ids_.size(); }
    // A closed receipt; nullptr when it is not in the journal (still open or unknown).
    [[nodiscard]] const Receipt* findReceipt(const Uuid& id) const noexcept;

    // What we owe the supplier: positive = we owe them, negative = they were overpaid.
    [[nodiscard]] Money balanceOf(SupplierId supplierId) const noexcept;
    // Receipts and payments of the supplier, oldest first: where the balance comes
    // from. Pointers are valid until the next add().
    [[nodiscard]] std::vector<const Transaction*> historyOf(SupplierId supplierId) const;
    // Everything put into the truck, from receipts and from the warehouse;
    // compare with Truck::boxLimit() to spot overload.
    [[nodiscard]] Cargo loadOf(const TruckId& truckId) const noexcept;
    // What the warehouse holds of the crop across all sessions: put in minus shipped out.
    // Negative after an offline conflict (two terminals shipped the same boxes).
    [[nodiscard]] Cargo stockOf(std::string_view cropCode) const noexcept;
    // Closed receipts and payments of one work day (their sessionId), in the order
    // added. Pointers are valid until the next add().
    [[nodiscard]] std::vector<const Receipt*> receiptsOf(const Uuid& sessionId) const;
    [[nodiscard]] std::vector<const Payment*> paymentsOf(const Uuid& sessionId) const;
    // What allocations still in force took from the receipt (closed or open), by kind of place.
    [[nodiscard]] Placement placementOf(const Uuid& receiptId) const noexcept;

    // Whether any record names the crop, cancelled receipt lines included.
    [[nodiscard]] bool mentionsCrop(std::string_view cropCode) const noexcept;

    // Part of the crop in the receipt that no truck has taken yet.
    // Throws std::invalid_argument when the receipt is not in the journal.
    [[nodiscard]] Cargo unallocatedOf(const Uuid& receiptId, std::string_view cropCode) const;
    // Same for a receipt still being weighed: it is not in the journal yet,
    // but its pallets may already be in a truck.
    [[nodiscard]] Cargo unallocatedOf(const OpenReceipt& receipt, std::string_view cropCode) const noexcept;

    // Where a whole weighing went; nullopt when it is still free. Boxes
    // allocated by count are not tied to a weighing and do not show here.
    [[nodiscard]] std::optional<Destination> destinationOf(const Uuid& receiptId,
                                                           std::size_t weighingIndex) const noexcept;
    // The allocation that took the whole weighing; nullptr while it is free.
    // Pointer valid until the next add().
    [[nodiscard]] const Allocation* allocationOf(const Uuid& receiptId, std::size_t weighingIndex) const noexcept;
    // Closed receipts with boxes that neither a truck nor the warehouse has
    // taken yet, oldest first: the allocation screen's to-do list. Pointers
    // are valid until the next add().
    [[nodiscard]] std::vector<const Receipt*> waitingForAllocation() const;

    // Preferred way to allocate boxes: the weight is split from what is left,
    // not from the whole crop. Still check it with validateAllocation().
    // Throws std::invalid_argument when the receipt is not in the journal.
    [[nodiscard]] Allocation boxesOf(const Uuid& receiptId, std::string_view cropCode, int boxes,
                                     Destination destination) const;
    [[nodiscard]] Allocation boxesOf(const OpenReceipt& receipt, std::string_view cropCode, int boxes,
                                     Destination destination) const;

    // Operator-facing check before a reversal is added: the allocation is known
    // and not taken back yet, and warehouse stock still holds its boxes.
    [[nodiscard]] std::vector<std::string> validateReversal(const AllocationReversal& reversal) const;

    // Operator-facing check before an allocation is added: its own errors plus
    // the rules that need the rest of the journal. add() does not call this on
    // purpose: facts synced from another terminal are accepted as they are.
    [[nodiscard]] std::vector<std::string> validateAllocation(const Allocation& allocation) const;
    // Same for an allocation from a receipt still open. Throws
    // std::invalid_argument when the allocation is from another receipt.
    [[nodiscard]] std::vector<std::string> validateAllocation(const Allocation& allocation,
                                                              const OpenReceipt& receipt) const;

    // Operator-facing: why a weighing of an open receipt cannot be cancelled.
    // A pallet allocated as a whole is cancelled together with its allocation
    // (see reversalOf), so its reversal must be possible; boxes allocated by
    // count must stay covered. Whether the truck still loads is WorkDay's.
    // Throws std::out_of_range for an index outside the receipt.
    [[nodiscard]] std::vector<std::string> validateCancel(const OpenReceipt& receipt, std::size_t index) const;
    // What frees a whole pallet again; nullopt while it is not allocated.
    [[nodiscard]] std::optional<AllocationReversal> reversalOf(const Uuid& receiptId, std::size_t weighingIndex) const;

    // Preferred way to ship from the warehouse: the weight is split from the
    // stock, so shipping the last boxes takes exactly the remaining weight.
    [[nodiscard]] StockShipment shipmentOf(std::string_view cropCode, int boxes, const TruckId& truckId) const;
    // Operator-facing check, like validateAllocation(): not more than is in stock.
    [[nodiscard]] std::vector<std::string> validateShipment(const StockShipment& shipment) const;

private:
    // Remembers the id; false when it was already there.
    bool registerId(const Uuid& id);
    // Allocations not taken back by a reversal: the only ones that move berries.
    [[nodiscard]] auto activeAllocations() const noexcept {
        return allocations_ | std::views::filter([this](const Allocation& a) { return !reversed_.contains(a.id()); });
    }
    // What allocations have already taken of the crop from the receipt.
    [[nodiscard]] Cargo allocatedOf(const Uuid& receiptId, std::string_view cropCode) const noexcept;
    // The checks shared by closed and open receipts.
    template <WeighedReceipt R>
    [[nodiscard]] std::vector<std::string> checkAllocation(const Allocation& allocation, const R& receipt) const;

    std::set<Uuid> ids_;
    std::vector<Receipt> receipts_;
    std::vector<Payment> payments_;
    std::vector<Allocation> allocations_;
    std::vector<StockShipment> shipments_;
    std::vector<AllocationReversal> reversals_;
    // Ids of the allocations reversals point at; a reversal synced in before
    // its allocation is already counted when the allocation comes.
    std::set<Uuid> reversed_;
};

} // namespace wms
