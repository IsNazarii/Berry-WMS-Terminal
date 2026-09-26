#include "wms/workday.h"

#include <algorithm>
#include <format>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace wms {

namespace {

bool isBlank(std::string_view text) {
    return std::ranges::all_of(text, [](char c) { return c == ' ' || c == '\t' || c == '\n'; });
}

} // namespace

WorkDay::WorkDay(std::chrono::year_month_day date, TimePoint openedAt, OperatorId openedBy)
    : date_{date}, openedAt_{openedAt}, openedBy_{openedBy} {
    if (!date_.ok()) throw std::invalid_argument("Work day needs a valid date");
    if (openedBy_ <= 0) throw std::invalid_argument("Work day must be opened by an operator");
}

void WorkDay::setPrice(std::string cropCode, Money pricePerKg, TimePoint at) {
    requireOpen();
    prices_.set(std::move(cropCode), pricePerKg, at);
}

const Truck* WorkDay::findTruck(const TruckId& id) const noexcept {
    const auto found = std::ranges::find(trucks_, id, &Truck::id);
    return found != trucks_.end() ? &*found : nullptr;
}

const Truck& WorkDay::addTruck(std::string plate, Cargo capacity) {
    requireOpen();
    return trucks_.emplace_back(Truck::create(std::move(plate), capacity));
}

const Truck& WorkDay::repeatTruck(const Truck& earlier) { return addTruck(earlier.plate(), earlier.capacity()); }

const Truck& WorkDay::adoptTruck(const Truck& carriedOver) {
    requireOpen();
    if (findTruck(carriedOver.id())) throw std::invalid_argument("Truck is already in this session");
    // A fresh object with the same id: it is loading again in this session.
    return trucks_.emplace_back(carriedOver.id(), carriedOver.plate(), carriedOver.capacity());
}

void WorkDay::departTruck(const TruckId& id) {
    requireOpen();
    truck(id).depart();
}

void WorkDay::carryOverTruck(const TruckId& id) {
    requireOpen();
    truck(id).carryOver();
}

std::vector<std::string> WorkDay::validateClose() const {
    std::vector<std::string> errors;
    for (const Truck& t : trucks_) {
        if (t.status() == TruckStatus::Loading) {
            errors.push_back(std::format("Фура {} ще завантажується: закрийте її або перенесіть у наступну сесію",
                                         t.plate()));
        }
    }
    return errors;
}

void WorkDay::close(TimePoint at, OperatorId by) {
    requireOpen();
    if (by <= 0) throw std::invalid_argument("Work day must be closed by an operator");
    if (!validateClose().empty()) throw std::logic_error("Every loading truck needs a decision before closing");
    closedAt_ = at;
    closedBy_ = by;
}

template <class Change>
void WorkDay::amend(const Amendment& amendment, std::string description, Change change) {
    if (amendment.by <= 0) throw std::invalid_argument("Amendment needs an operator");
    if (isBlank(amendment.reason)) throw std::invalid_argument("Amendment needs a reason");
    // Apply first: if the change throws, nothing is logged.
    change();
    amendments_.push_back({amendment, std::move(description)});
}

void WorkDay::setPrice(std::string cropCode, Money pricePerKg, TimePoint at, const Amendment& amendment) {
    auto description = std::format("Ціна {}: {}", cropCode, formatMoney(pricePerKg));
    amend(amendment, std::move(description), [&] { prices_.set(std::move(cropCode), pricePerKg, at); });
}

const Truck& WorkDay::addTruck(std::string plate, Cargo capacity, const Amendment& amendment) {
    auto description = std::format("Додано фуру {}", plate);
    const Truck* added = nullptr;
    amend(amendment, std::move(description),
          [&] { added = &trucks_.emplace_back(Truck::create(std::move(plate), capacity)); });
    return *added;
}

void WorkDay::departTruck(const TruckId& id, const Amendment& amendment) {
    Truck& t = truck(id);
    amend(amendment, std::format("Фуру {} відправлено", t.plate()), [&] { t.depart(); });
}

void WorkDay::carryOverTruck(const TruckId& id, const Amendment& amendment) {
    Truck& t = truck(id);
    amend(amendment, std::format("Фуру {} перенесено в наступну сесію", t.plate()), [&] { t.carryOver(); });
}

std::vector<Truck> WorkDay::trucksForNextSession() const {
    std::vector<Truck> result;
    std::ranges::copy_if(trucks_, std::back_inserter(result),
                         [](const Truck& t) { return t.status() == TruckStatus::CarriedOver; });
    return result;
}

void WorkDay::requireOpen() const {
    if (!isOpen()) throw std::logic_error("Work day is already closed");
}

Truck& WorkDay::truck(const TruckId& id) {
    const auto found = std::ranges::find(trucks_, id, &Truck::id);
    if (found == trucks_.end()) throw std::invalid_argument("Truck is not in this session");
    return *found;
}

} // namespace wms
