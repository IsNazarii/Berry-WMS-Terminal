#include "wms/display_format.h"

#include <cstdint>
#include <string>

namespace wms {
namespace {

constexpr const char* kNarrowSpace = " ";
constexpr const char* kMinus = "−";

std::string groupThousands(std::int64_t whole) {
    std::string digits = std::to_string(whole);
    for (auto i = static_cast<std::int64_t>(digits.size()) - 3; i > 0; i -= 3) {
        digits.insert(static_cast<std::size_t>(i), kNarrowSpace);
    }
    return digits;
}

std::string fixed(std::int64_t raw, std::int64_t scale, std::size_t decimals, std::size_t minDecimals) {
    const bool negative = raw < 0;
    const std::int64_t value = negative ? -raw : raw;
    std::string fraction = std::to_string(value % scale);
    fraction.insert(0, decimals - fraction.size(), '0');
    while (fraction.size() > minDecimals && fraction.back() == '0') fraction.pop_back();
    std::string text = groupThousands(value / scale);
    if (!fraction.empty()) text += ',' + fraction;
    return negative ? kMinus + text : text;
}

} // namespace

std::string displayKilograms(Weight weight) { return fixed(weight.raw(), 1000, 3, 1); }

std::string displayHryvnias(Money money) { return fixed(money.raw(), 100, 2, 2); }

} // namespace wms
