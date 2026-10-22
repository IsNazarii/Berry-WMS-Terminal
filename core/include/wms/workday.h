#pragma once

#include "wms/journal.h"
#include "wms/open_receipt.h"
#include "wms/operator.h"
#include "wms/pricing.h"
#include "wms/truck.h"

#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace wms {

// A change to a closed session: allowed, but it must say who made it, when and why.
struct Amendment {
    OperatorId by = 0;
    TimePoint at;
    std::string reason;
};

struct AmendmentRecord {
    Amendment amendment;
    std::string change; // what was changed, written by WorkDay itself
};

// Everything a session consists of, as storage keeps it; see WorkDay::restore().
struct WorkDayRecord {
    Uuid id;
    std::chrono::year_month_day date;
    TimePoint openedAt;
    OperatorId openedBy = 0;
    std::optional<TimePoint> closedAt;
    std::optional<OperatorId> closedBy;
    DayPrices prices;
    std::vector<Truck> trucks;
    std::vector<AmendmentRecord> amendments;
    std::vector<OpenReceipt> openReceipts;
};

// One working session of the collection point. It opens around noon, may run
// until the next morning and is named by the date it opened. Suppliers and
// their debts live in the journal and span all sessions; prices and trucks
// belong to a session.
class WorkDay {
public:
    // The date is passed in rather than derived from `openedAt`: the core does
    // not know the local time zone, the UI does.
    // The session gets a fresh UUIDv7: sessions are opened offline, and two
    // may share a date (closed by mistake and opened again).
    WorkDay(std::chrono::year_month_day date, TimePoint openedAt, OperatorId openedBy);

    // A session read back from storage, open or closed, exactly as it was:
    // nothing is re-checked against the rules that let it get there. Throws
    // std::invalid_argument for a record no session could be in (closed only
    // half-way, or closed with receipts still open).
    [[nodiscard]] static WorkDay restore(WorkDayRecord record);

    [[nodiscard]] const Uuid& id() const noexcept { return id_; }

    [[nodiscard]] std::chrono::year_month_day date() const noexcept { return date_; }
    [[nodiscard]] TimePoint openedAt() const noexcept { return openedAt_; }
    [[nodiscard]] std::optional<TimePoint> closedAt() const noexcept { return closedAt_; }
    [[nodiscard]] bool isOpen() const noexcept { return !closedAt_.has_value(); }
    [[nodiscard]] OperatorId openedBy() const noexcept { return openedBy_; }
    [[nodiscard]] std::optional<OperatorId> closedBy() const noexcept { return closedBy_; }

    [[nodiscard]] const DayPrices& prices() const noexcept { return prices_; }
    // Changes of an open session only; throws std::logic_error once it is closed.
    void setPrice(std::string cropCode, Money pricePerKg, TimePoint at);

    [[nodiscard]] const std::vector<Truck>& trucks() const noexcept { return trucks_; }
    [[nodiscard]] const Truck* findTruck(const TruckId& id) const noexcept;

    // All of these throw std::logic_error once the session is closed.
    const Truck& addTruck(std::string plate, int boxLimit, TruckDetails details = {});
    // Same plate, box limit and details as a truck of an earlier session, but a
    // new trip: new id, empty.
    const Truck& repeatTruck(const Truck& earlier);
    // A truck carried over from the previous session: same id, so its load so far still counts.
    const Truck& adoptTruck(const Truck& carriedOver);

    // The operator's decision for every truck still loading when the session
    // ends. Throw std::invalid_argument for a truck of another session.
    void departTruck(const TruckId& id);
    void carryOverTruck(const TruckId& id);

    // Receipts being weighed right now, several at once. They belong to the
    // session: it cannot be closed while any of them is open.
    [[nodiscard]] const std::vector<OpenReceipt>& openReceipts() const noexcept { return openReceipts_; }
    [[nodiscard]] const OpenReceipt* findReceipt(const Uuid& id) const noexcept;
    // Whether an open receipt names the crop, cancelled lines included; closed
    // receipts are the journal's (Journal::mentionsCrop).
    [[nodiscard]] bool mentionsCrop(std::string_view cropCode) const noexcept;

    // The supplier's open receipt, or a new one: a supplier never has two.
    // The reference is valid until the next change of open receipts.
    // Throws std::logic_error once the session is closed.
    const OpenReceipt& openReceipt(SupplierId supplierId);
    // A weighing at the day price takes the price in effect at `at`; a manual
    // price is kept. Without a day price the line stays unpriced: fine for
    // wholesale, a retail receipt will not close. Returns the line index.
    // All of these throw std::invalid_argument for a receipt that is not open.
    std::size_t addWeighing(const Uuid& receiptId, Weighing weighing, TimePoint at);
    // Operator-facing: Journal::validateCancel() plus the truck: berries that
    // left with it cannot be taken back.
    [[nodiscard]] std::vector<std::string> validateCancel(const Uuid& receiptId, std::size_t index,
                                                          const Journal& journal) const;
    // Returns the reversal of the pallet's allocation, if it had one: the
    // caller puts it into the journal together with saving the session.
    // Throws std::logic_error when validateCancel() is not empty.
    [[nodiscard]] std::optional<AllocationReversal> cancelWeighing(const Uuid& receiptId, std::size_t index,
                                                                   const Journal& journal);

    // Auto-allocation from the receiving screen. A pallet goes where the last
    // allocated pallet of its crop in this receipt went, so one supplier's
    // berries stay together; otherwise to `target`, the operator's choice.
    // A truck that left no longer leads. nullopt when there is nowhere to go.
    [[nodiscard]] std::optional<Destination> nextDestination(const Uuid& receiptId, std::string_view cropCode,
                                                             std::optional<Destination> target,
                                                             const Journal& journal) const;
    // The allocation for the pallet just weighed; nullopt leaves it hanging for
    // the allocation screen: nowhere to go, or the truck has no room for all
    // its boxes. Never spills into another truck: the operator decides that.
    [[nodiscard]] std::optional<Allocation> autoAllocation(const Uuid& receiptId, std::size_t index,
                                                           std::optional<Destination> target,
                                                           const Journal& journal) const;
    // The receipt leaves the open list; the caller puts it into the journal.
    // Throws std::logic_error when OpenReceipt::validateClose() is not empty.
    [[nodiscard]] Receipt closeReceipt(const Uuid& receiptId, Settlement settlement);

    // Operator-facing: why the session cannot be closed yet; empty when it can.
    [[nodiscard]] std::vector<std::string> validateClose() const;
    // Throws std::logic_error when validateClose() is not empty or already closed.
    void close(TimePoint at, OperatorId by);

    // The same changes as an amendment: allowed in a closed session too, and
    // logged with what was changed. Throw std::invalid_argument without an
    // operator or a reason; a change that fails is not logged.
    void setPrice(std::string cropCode, Money pricePerKg, TimePoint at, const Amendment& amendment);
    const Truck& addTruck(std::string plate, int boxLimit, const Amendment& amendment, TruckDetails details = {});
    void departTruck(const TruckId& id, const Amendment& amendment);
    void carryOverTruck(const TruckId& id, const Amendment& amendment);
    [[nodiscard]] const std::vector<AmendmentRecord>& amendments() const noexcept { return amendments_; }

    [[nodiscard]] std::vector<Truck> trucksForNextSession() const;

private:
    void requireOpen() const;
    // Checks the amendment, applies `change` and logs it with `description`.
    template <class Change>
    void amend(const Amendment& amendment, std::string description, Change change);
    [[nodiscard]] Truck& truck(const TruckId& id);
    [[nodiscard]] std::vector<OpenReceipt>::iterator receipt(const Uuid& id);
    [[nodiscard]] const OpenReceipt& receipt(const Uuid& id) const;
    // A truck of this session that still loads; the warehouse always takes berries.
    [[nodiscard]] bool accepts(const Destination& destination) const noexcept;

    Uuid id_;
    std::chrono::year_month_day date_;
    TimePoint openedAt_;
    OperatorId openedBy_;
    std::optional<TimePoint> closedAt_;
    std::optional<OperatorId> closedBy_;
    DayPrices prices_;
    std::vector<Truck> trucks_;
    std::vector<AmendmentRecord> amendments_;
    std::vector<OpenReceipt> openReceipts_;
};

} // namespace wms
