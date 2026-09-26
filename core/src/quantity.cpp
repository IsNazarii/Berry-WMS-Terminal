#include "wms/quantity.h"

#include <charconv>
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

std::string_view trim(std::string_view text) {
    const auto first = text.find_first_not_of(' ');
    if (first == std::string_view::npos) return {};
    return text.substr(first, text.find_last_not_of(' ') - first + 1);
}

// Digits only: from_chars alone would also take a leading minus.
std::optional<std::int64_t> parseDigits(std::string_view digits) {
    if (digits.empty() || digits.find_first_not_of("0123456789") != std::string_view::npos) return std::nullopt;
    std::int64_t value = 0;
    const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), value);
    if (error != std::errc{}) return std::nullopt; // out of range
    return value;
}

} // namespace

std::optional<Money> parseMoney(std::string_view text) {
    text = trim(text);
    const auto separator = text.find_first_of(".,");
    const auto whole = parseDigits(text.substr(0, separator));
    if (!whole || *whole > INT64_MAX / 100) return std::nullopt;
    if (separator == std::string_view::npos) return hryvnias(*whole);

    const auto fractionText = text.substr(separator + 1);
    const auto fraction = parseDigits(fractionText);
    if (!fraction || fractionText.size() > 2) return std::nullopt;
    // "85.5" means 50 kopecks, not 5.
    return hryvnias(*whole) + kopecks(fractionText.size() == 1 ? *fraction * 10 : *fraction);
}

std::string formatMoney(Money money) { return formatFixed(money.raw(), 100, 2, " грн"); }

std::string formatWeight(Weight weight) { return formatFixed(weight.raw(), 1000, 3, " кг"); }

} // namespace wms
