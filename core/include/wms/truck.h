#pragma once

#include "wms/quantity.h"

#include <cstdint>
#include <string>

namespace wms {

using TruckId = std::int64_t;

enum class TruckStatus { Loading, Departed };

// Amount of berries measured both ways a truck can be limited by.
struct Cargo {
    int boxes = 0;
    Weight weight;

    Cargo& operator+=(const Cargo& other) noexcept {
        boxes += other.boxes;
        weight += other.weight;
        return *this;
    }

    bool operator==(const Cargo&) const noexcept = default;
};

// A truck taking berries from the collection point. The current load is not
// stored here: it is derived from allocation records, like the supplier balance,
// so two offline terminals never overwrite each other's counters.
class Truck {
public:
    // A zero limit in `capacity` means the truck is not limited in that dimension;
    // at least one limit must be set.
    Truck(TruckId id, std::string plate, Cargo capacity);

    [[nodiscard]] TruckId id() const noexcept { return id_; }
    [[nodiscard]] const std::string& plate() const noexcept { return plate_; }
    [[nodiscard]] const Cargo& capacity() const noexcept { return capacity_; }
    [[nodiscard]] TruckStatus status() const noexcept { return status_; }

    [[nodiscard]] bool acceptsCargo() const noexcept { return status_ == TruckStatus::Loading; }
    void depart();

    // Negative values mean the truck is overloaded in that dimension.
    [[nodiscard]] Cargo remaining(const Cargo& loaded) const noexcept;
    [[nodiscard]] bool isOverloaded(const Cargo& loaded) const noexcept;
    // Fill level by the tighter of the two limits; may exceed 100 when overloaded.
    [[nodiscard]] int fillPercent(const Cargo& loaded) const noexcept;

private:
    TruckId id_;
    std::string plate_;
    Cargo capacity_;
    TruckStatus status_ = TruckStatus::Loading;
};

} // namespace wms
