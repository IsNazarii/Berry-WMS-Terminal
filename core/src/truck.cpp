#include "wms/truck.h"

#include <stdexcept>
#include <utility>

namespace wms {

Truck::Truck(TruckId id, std::string plate, int boxLimit, TruckDetails details, TruckStatus status)
    : id_{id}, plate_{std::move(plate)}, boxLimit_{boxLimit}, details_{std::move(details)}, status_{status} {
    if (id_.isNil()) throw std::invalid_argument("Truck id must not be nil");
    if (plate_.empty()) throw std::invalid_argument("Truck plate must not be empty");
    if (boxLimit_ <= 0) throw std::invalid_argument("Truck box limit must be positive");
}

Truck Truck::create(std::string plate, int boxLimit, TruckDetails details) {
    return Truck{Uuid::generateV7(), std::move(plate), boxLimit, std::move(details)};
}

void Truck::depart() {
    if (status_ != TruckStatus::Loading) throw std::logic_error("Only a loading truck can depart");
    status_ = TruckStatus::Departed;
}

void Truck::carryOver() {
    if (status_ != TruckStatus::Loading) throw std::logic_error("Only a loading truck can be carried over");
    status_ = TruckStatus::CarriedOver;
}

int Truck::boxesLeft(const Cargo& loaded) const noexcept { return boxLimit_ - loaded.boxes; }

bool Truck::isOverloaded(const Cargo& loaded) const noexcept { return loaded.boxes > boxLimit_; }

int Truck::fillPercent(const Cargo& loaded) const noexcept {
    return static_cast<int>(divideRounded(std::int64_t{loaded.boxes} * 100, boxLimit_));
}

} // namespace wms
