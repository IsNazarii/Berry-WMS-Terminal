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

// A decimal typed by the operator, in units of 10^-fractionDigits: "85,5" with
// 2 digits is 8550. Either separator, since the Ukrainian keyboard gives a comma.
std::optional<std::int64_t> parseFixed(std::string_view text, int fractionDigits) {
    std::int64_t scale = 1;
    for (int i = 0; i < fractionDigits; ++i) scale *= 10;

    text = trim(text);
    const auto separator = text.find_first_of(".,");
    const auto whole = parseDigits(text.substr(0, separator));
    if (!whole || *whole > INT64_MAX / scale) return std::nullopt;
    if (separator == std::string_view::npos) return *whole * scale;

    const auto fractionText = text.substr(separator + 1);
    auto fraction = parseDigits(fractionText);
    if (!fraction || fractionText.size() > static_cast<std::size_t>(fractionDigits)) return std::nullopt;
    // "85.5" means 50 kopecks, not 5: pad the fraction to full length.
    for (auto i = fractionText.size(); i < static_cast<std::size_t>(fractionDigits); ++i) *fraction *= 10;
    return *whole * scale + *fraction;
}

} // namespace

std::optional<Money> parseMoney(std::string_view text) {
    const auto raw = parseFixed(text, 2);
    return raw ? std::optional{kopecks(*raw)} : std::nullopt;
}

std::optional<Weight> parseWeight(std::string_view text) {
    const auto raw = parseFixed(text, 3);
    return raw ? std::optional{grams(*raw)} : std::nullopt;
}

std::string formatMoney(Money money) { return formatFixed(money.raw(), 100, 2, " грн"); }

std::string formatWeight(Weight weight) { return formatFixed(weight.raw(), 1000, 3, " кг"); }

} // namespace wms
