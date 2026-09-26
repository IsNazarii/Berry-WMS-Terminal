#pragma once

#include "wms/quantity.h"

namespace wms {

// Amount of berries measured both ways a truck can be limited by.
struct Cargo {
    int boxes = 0;
    Weight weight;

    Cargo& operator+=(const Cargo& other) noexcept {
        boxes += other.boxes;
        weight += other.weight;
        return *this;
    }

    Cargo& operator-=(const Cargo& other) noexcept {
        boxes -= other.boxes;
        weight -= other.weight;
        return *this;
    }

    bool operator==(const Cargo&) const noexcept = default;
};

} // namespace wms
