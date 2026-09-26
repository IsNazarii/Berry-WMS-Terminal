#pragma once

#include "wms/pricing.h"
#include "wms/truck.h"

#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace wms {

// One working session of the collection point. It opens around noon, may run
// until the next morning and is named by the date it opened. Suppliers and
// their debts live in the journal and span all sessions; prices and trucks
// belong to a session.
class WorkDay {
public:
    // The date is passed in rather than derived from `openedAt`: the core does
    // not know the local time zone, the UI does.
    WorkDay(std::chrono::year_month_day date, TimePoint openedAt);

    [[nodiscard]] std::chrono::year_month_day date() const noexcept { return date_; }
    [[nodiscard]] TimePoint openedAt() const noexcept { return openedAt_; }
    [[nodiscard]] std::optional<TimePoint> closedAt() const noexcept { return closedAt_; }
    [[nodiscard]] bool isOpen() const noexcept { return !closedAt_.has_value(); }

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

    // Operator-facing: why the session cannot be closed yet; empty when it can.
    [[nodiscard]] std::vector<std::string> validateClose() const;
    // Throws std::logic_error when validateClose() is not empty or already closed.
    void close(TimePoint at);

    [[nodiscard]] std::vector<Truck> trucksForNextSession() const;

private:
    void requireOpen() const;
    [[nodiscard]] Truck& truck(const TruckId& id);

    std::chrono::year_month_day date_;
    TimePoint openedAt_;
    std::optional<TimePoint> closedAt_;
    DayPrices prices_;
    std::vector<Truck> trucks_;
};

} // namespace wms
