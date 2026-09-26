#include "wms/truck.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace wms {

Truck::Truck(TruckId id, std::string plate, Cargo capacity)
    : id_{id}, plate_{std::move(plate)}, capacity_{capacity} {
    if (id_.isNil()) throw std::invalid_argument("Truck id must not be nil");
    if (plate_.empty()) throw std::invalid_argument("Truck plate must not be empty");
    if (capacity_.boxes < 0 || capacity_.weight.isNegative())
        throw std::invalid_argument("Truck capacity must not be negative");
    if (capacity_.boxes == 0 && capacity_.weight.isZero())
        throw std::invalid_argument("Truck must be limited by boxes, weight or both");
}

Truck Truck::create(std::string plate, Cargo capacity) {
    return Truck{Uuid::generateV7(), std::move(plate), capacity};
}

void Truck::depart() {
    if (status_ != TruckStatus::Loading) throw std::logic_error("Only a loading truck can depart");
    status_ = TruckStatus::Departed;
}

void Truck::carryOver() {
    if (status_ != TruckStatus::Loading) throw std::logic_error("Only a loading truck can be carried over");
    status_ = TruckStatus::CarriedOver;
}

Cargo Truck::remaining(const Cargo& loaded) const noexcept {
    return {.boxes = capacity_.boxes - loaded.boxes, .weight = capacity_.weight - loaded.weight};
}

bool Truck::isOverloaded(const Cargo& loaded) const noexcept {
    const bool tooManyBoxes = capacity_.boxes > 0 && loaded.boxes > capacity_.boxes;
    const bool tooHeavy = capacity_.weight.isPositive() && loaded.weight > capacity_.weight;
    return tooManyBoxes || tooHeavy;
}

int Truck::fillPercent(const Cargo& loaded) const noexcept {
    std::int64_t percent = 0;
    if (capacity_.boxes > 0) {
        percent = std::max(percent, divideRounded(std::int64_t{loaded.boxes} * 100, capacity_.boxes));
    }
    if (capacity_.weight.isPositive()) {
        percent = std::max(percent, divideRounded(loaded.weight.raw() * 100, capacity_.weight.raw()));
    }
    return static_cast<int>(percent);
}

} // namespace wms
