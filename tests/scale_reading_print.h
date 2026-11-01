#pragma once

#include "wms/scale.h"

#include <ostream>

namespace wms {
// Readable gtest failure messages instead of a byte dump.
inline void PrintTo(const ScaleReading& r, std::ostream* os) {
    *os << r.weight.raw() << " g" << (r.stable ? " stable" : "") << (r.zero ? " zero" : "")
        << (r.tare ? " tare" : "");
}
} // namespace wms
