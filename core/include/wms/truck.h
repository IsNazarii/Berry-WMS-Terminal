#pragma once

#include "wms/cargo.h"
#include "wms/uuid.h"

#include <string>

namespace wms {

// Trucks are created by the operator on any terminal, offline too, so their ids
// are UUIDs like the records: two terminals never hand out the same id.
using TruckId = Uuid;

enum class TruckStatus { Loading, Departed };

// A truck taking berries from the collection point. The current load is not
// stored here: it is derived from allocation records, like the supplier balance,
// so two offline terminals never overwrite each other's counters.
class Truck {
public:
    // A zero limit in `capacity` means the truck is not limited in that dimension;
    // at least one limit must be set.
    Truck(TruckId id, std::string plate, Cargo capacity);

    // New truck with a fresh UUIDv7.
    [[nodiscard]] static Truck create(std::string plate, Cargo capacity);

    [[nodiscard]] const TruckId& id() const noexcept { return id_; }
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
