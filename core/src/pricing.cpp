#include "wms/pricing.h"

#include <stdexcept>
#include <utility>

namespace wms {

void DayPrices::set(std::string cropCode, Money pricePerKg, TimePoint at) {
    if (cropCode.empty()) throw std::invalid_argument("Price change needs a crop");
    if (!pricePerKg.isPositive()) throw std::invalid_argument("Price must be positive");
    history_.push_back({std::move(cropCode), pricePerKg, at});
}

std::optional<Money> DayPrices::priceAt(std::string_view cropCode, TimePoint at) const noexcept {
    const PriceChange* best = nullptr;
    for (const auto& change : history_) {
        if (change.cropCode != cropCode || change.at > at) continue;
        // `>=`: at the same moment the later record wins (a corrected typo).
        if (!best || change.at >= best->at) best = &change;
    }
    return best ? std::optional{best->pricePerKg} : std::nullopt;
}

std::optional<Money> DayPrices::current(std::string_view cropCode) const noexcept {
    return priceAt(cropCode, TimePoint::max());
}

} // namespace wms
