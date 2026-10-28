#pragma once

#include "wms/cargo.h"
#include "wms/journal.h"
#include "wms/quantity.h"
#include "wms/workday.h"

#include <string>
#include <vector>

namespace wms {

struct CropTotal {
    std::string cropCode;
    Cargo cargo;
};

// What a work day has taken in and paid out, for the session screen. Built from
// the records that name the session (sessionId), not from a time range: a late
// receipt still counts for its own day, and a wrong clock changes nothing.
// Receipts still being weighed count too: the berries are on the point already.
// A snapshot: build a new one after the journal or the day changes.
class SessionSummary {
public:
    SessionSummary(const WorkDay& day, const Journal& journal);

    // Without cancelled lines.
    [[nodiscard]] Cargo received() const noexcept { return received_; }
    // The heaviest crop first.
    [[nodiscard]] const std::vector<CropTotal>& byCrop() const noexcept { return byCrop_; }
    [[nodiscard]] Cargo inTrucks() const noexcept { return placed_.inTrucks; }
    [[nodiscard]] Cargo inWarehouse() const noexcept { return placed_.inWarehouse; }
    // Received but placed nowhere yet: what the allocation screen still has to do.
    [[nodiscard]] Cargo waiting() const noexcept;

    [[nodiscard]] int closedReceipts() const noexcept { return closedReceipts_; }
    [[nodiscard]] int openReceipts() const noexcept { return openReceipts_; }

    // Everything paid out during the day, old debts included: it is what left the till.
    [[nodiscard]] Money paidCash() const noexcept { return paidCash_; }
    [[nodiscard]] Money paidTransfer() const noexcept { return paidTransfer_; }
    [[nodiscard]] Money paid() const noexcept { return paidCash_ + paidTransfer_; }

private:
    // Adds the counted lines of a receipt, closed or open, and where they went.
    template <WeighedReceipt R>
    void count(const R& receipt, const Journal& journal);

    Cargo received_;
    std::vector<CropTotal> byCrop_;
    Placement placed_;
    int closedReceipts_ = 0;
    int openReceipts_ = 0;
    Money paidCash_;
    Money paidTransfer_;
};

} // namespace wms
