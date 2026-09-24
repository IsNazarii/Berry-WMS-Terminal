#include "wms/quantity.h"

#include <format>
#include <string_view>

namespace wms {
namespace {

// Formats a fixed-point integer, e.g. (123450, 100, 2, " грн") -> "1234.50 грн".
std::string formatFixed(std::int64_t raw, std::uint64_t scale, int fractionDigits, std::string_view unit) {
    const bool negative = raw < 0;
    // Negate in unsigned arithmetic so INT64_MIN does not overflow.
    const std::uint64_t magnitude = negative ? 0 - static_cast<std::uint64_t>(raw) : static_cast<std::uint64_t>(raw);
    return std::format("{}{}.{:0{}}{}", negative ? "-" : "", magnitude / scale, magnitude % scale, fractionDigits,
                       unit);
}

} // namespace

std::string formatMoney(Money money) { return formatFixed(money.raw(), 100, 2, " грн"); }

std::string formatWeight(Weight weight) { return formatFixed(weight.raw(), 1000, 3, " кг"); }

} // namespace wms
