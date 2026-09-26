#pragma once

#include "wms/journal.h"
#include "wms/open_receipt.h"
#include "wms/operator.h"
#include "wms/pricing.h"
#include "wms/truck.h"

#include <chrono>
#include <optional>
#include <string>
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

// One working session of the collection point. It opens around noon, may run
// until the next morning and is named by the date it opened. Suppliers and
// their debts live in the journal and span all sessions; prices and trucks
// belong to a session.
class WorkDay {
public:
    // The date is passed in rather than derived from `openedAt`: the core does
    // not know the local time zone, the UI does.
    WorkDay(std::chrono::year_month_day date, TimePoint openedAt, OperatorId openedBy);

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
    const Truck& addTruck(std::string plate, Cargo capacity);
    // Same plate and limits as a truck of an earlier session, but a new trip:
    // new id, empty.
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

    // The supplier's open receipt, or a new one: a supplier never has two.
    // The reference is valid until the next change of open receipts.
    // Throws std::logic_error once the session is closed.
    const OpenReceipt& openReceipt(SupplierId supplierId);
    // A weighing at the day price takes the price in effect at `at`; a manual
    // price is kept. Without a day price the line stays unpriced: fine for
    // wholesale, a retail receipt will not close. Returns the line index.
    // All of these throw std::invalid_argument for a receipt that is not open.
    std::size_t addWeighing(const Uuid& receiptId, Weighing weighing, TimePoint at);
    // Throws std::logic_error when Journal::validateCancel() is not empty.
    void cancelWeighing(const Uuid& receiptId, std::size_t index, const Journal& journal);
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
    const Truck& addTruck(std::string plate, Cargo capacity, const Amendment& amendment);
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
