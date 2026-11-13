#pragma once

#include "wms/allocation.h"
#include "wms/cargo.h"
#include "wms/journal.h"
#include "wms/open_receipt.h"
#include "wms/operator.h"
#include "wms/supplier.h"
#include "wms/workday.h"

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace wms {

// One move of berries as the history screen lists it: from a receipt into a
// truck or the warehouse (an Allocation), or out of the warehouse into a truck
// (a StockShipment).
struct LoadingEntry {
    Uuid recordId;
    TimePoint at;
    Destination to;
    std::string cropCode;
    Cargo cargo;
    // nullopt: shipped from the warehouse, where boxes no longer have a supplier.
    std::optional<Uuid> receiptId;
    // nullopt as well when the receipt is unknown here (a record synced before its receipt).
    std::optional<SupplierId> supplierId;
    // A whole pallet; nullopt: boxes counted out of the receipt's crop.
    std::optional<std::size_t> weighingIndex;
    // Who loaded it; nullopt for records from before operators were kept.
    std::optional<OperatorId> by;
    // Taken back by a reversal (a cancelled pallet): still listed, so the mistake can be seen.
    std::optional<TimePoint> reversedAt;
    std::optional<OperatorId> reversedBy;

    [[nodiscard]] bool fromWarehouse() const noexcept { return !receiptId.has_value(); }
    [[nodiscard]] bool reversed() const noexcept { return reversedAt.has_value(); }
};

// What a work day loaded and where from, oldest first. Built like SessionSummary:
// a snapshot of the journal and the day, rebuilt after either changes.
//
// A truck is listed for its whole trip: a truck carried over from yesterday
// shows yesterday's pallets too, because they leave with it. The warehouse is
// listed for the day only: what the day's receipts put in and what the day's
// trucks took out.
class LoadingHistory {
public:
    // `otherOpenReceipts`: receipts still being weighed in another session
    // (the current one, when `day` is from the archive), so a pallet they put
    // into a carried-over truck still shows its supplier.
    LoadingHistory(const WorkDay& day, const Journal& journal, std::span<const OpenReceipt> otherOpenReceipts = {});

    // Every move of the day: into its trucks and in and out of the warehouse.
    [[nodiscard]] const std::vector<LoadingEntry>& entries() const noexcept { return entries_; }
    // What went into one truck of the day; empty for a truck of another day.
    [[nodiscard]] std::vector<LoadingEntry> intoTruck(const TruckId& truckId) const;
    // What went into any truck of the day.
    [[nodiscard]] std::vector<LoadingEntry> intoTrucks() const;
    // What the day's receipts put into the warehouse and the day's trucks took from it.
    [[nodiscard]] std::vector<LoadingEntry> warehouse() const;

private:
    std::vector<LoadingEntry> entries_;
};

} // namespace wms
