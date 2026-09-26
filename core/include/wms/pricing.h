#pragma once

#include "wms/quantity.h"
#include "wms/transaction.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace wms {

struct PriceChange {
    std::string cropCode;
    Money pricePerKg;
    TimePoint at;
};

// Prices of one session. The operator may raise a price during the day:
// weighings after the change get the new price, earlier ones keep theirs.
// Changes are only appended, so it stays visible when and to what a price moved.
class DayPrices {
public:
    // Throws std::invalid_argument for an empty crop or a non-positive price.
    void set(std::string cropCode, Money pricePerKg, TimePoint at);

    // The price in effect at `at`: the latest change not after it. Decided by
    // time, not by recording order, since changes may arrive later via sync.
    [[nodiscard]] std::optional<Money> priceAt(std::string_view cropCode, TimePoint at) const noexcept;
    [[nodiscard]] std::optional<Money> current(std::string_view cropCode) const noexcept;
    [[nodiscard]] const std::vector<PriceChange>& history() const noexcept { return history_; }

private:
    std::vector<PriceChange> history_; // in recording order
};

} // namespace wms
