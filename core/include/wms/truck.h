#pragma once

#include "wms/cargo.h"
#include "wms/uuid.h"

#include <string>

namespace wms {

// Trucks are created by the operator on any terminal, offline too, so their ids
// are UUIDs like the records: two terminals never hand out the same id.
using TruckId = Uuid;

// CarriedOver: the session closed before the truck left; it goes on loading
// in the next session under the same id.
enum class TruckStatus { Loading, Departed, CarriedOver };

// What the operator notes about a truck besides its plate; both are free text
// and may stay empty. `note` is whatever helps to tell trips apart: the
// driver, where the truck goes.
struct TruckDetails {
    std::string make;
    std::string note;

    bool operator==(const TruckDetails&) const = default;
};

// A truck taking berries from the collection point. The current load is not
// stored here: it is derived from allocation records, like the supplier balance,
// so two offline terminals never overwrite each other's counters.
class Truck {
public:
    // A zero limit in `capacity` means the truck is not limited in that dimension;
    // at least one limit must be set. The status is for a truck read back from storage.
    Truck(TruckId id, std::string plate, Cargo capacity, TruckDetails details = {},
          TruckStatus status = TruckStatus::Loading);

    // New truck with a fresh UUIDv7.
    [[nodiscard]] static Truck create(std::string plate, Cargo capacity, TruckDetails details = {});

    [[nodiscard]] const TruckId& id() const noexcept { return id_; }
    [[nodiscard]] const std::string& plate() const noexcept { return plate_; }
    [[nodiscard]] const Cargo& capacity() const noexcept { return capacity_; }
    [[nodiscard]] const TruckDetails& details() const noexcept { return details_; }
    [[nodiscard]] TruckStatus status() const noexcept { return status_; }

    [[nodiscard]] bool acceptsCargo() const noexcept { return status_ == TruckStatus::Loading; }
    // Both throw std::logic_error unless the truck is still loading.
    void depart();
    void carryOver();

    // Negative values mean the truck is overloaded in that dimension.
    [[nodiscard]] Cargo remaining(const Cargo& loaded) const noexcept;
    [[nodiscard]] bool isOverloaded(const Cargo& loaded) const noexcept;
    // Fill level by the tighter of the two limits; may exceed 100 when overloaded.
    [[nodiscard]] int fillPercent(const Cargo& loaded) const noexcept;

private:
    TruckId id_;
    std::string plate_;
    Cargo capacity_;
    TruckDetails details_;
    TruckStatus status_ = TruckStatus::Loading;
};

} // namespace wms
