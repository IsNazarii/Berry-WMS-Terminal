#pragma once

#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace wms {

// Strongly typed integer quantity. The Tag parameter makes different units
// distinct types at compile time, so `Money + Weight` does not compile.
// Values are stored as int64 in the smallest unit to avoid float rounding errors.
template <class Tag>
class Quantity {
public:
    using Rep = std::int64_t;

    constexpr Quantity() noexcept = default;

    [[nodiscard]] static constexpr Quantity fromRaw(Rep raw) noexcept { return Quantity{raw}; }
    [[nodiscard]] constexpr Rep raw() const noexcept { return raw_; }

    [[nodiscard]] constexpr bool isZero() const noexcept { return raw_ == 0; }
    [[nodiscard]] constexpr bool isPositive() const noexcept { return raw_ > 0; }
    [[nodiscard]] constexpr bool isNegative() const noexcept { return raw_ < 0; }

    constexpr auto operator<=>(const Quantity&) const noexcept = default;

    constexpr Quantity operator-() const noexcept { return Quantity{-raw_}; }

    constexpr Quantity& operator+=(Quantity other) noexcept {
        raw_ += other.raw_;
        return *this;
    }

    constexpr Quantity& operator-=(Quantity other) noexcept {
        raw_ -= other.raw_;
        return *this;
    }

    friend constexpr Quantity operator+(Quantity a, Quantity b) noexcept { return a += b; }
    friend constexpr Quantity operator-(Quantity a, Quantity b) noexcept { return a -= b; }
    friend constexpr Quantity operator*(Quantity q, Rep factor) noexcept { return Quantity{q.raw_ * factor}; }
    friend constexpr Quantity operator*(Rep factor, Quantity q) noexcept { return q * factor; }

private:
    explicit constexpr Quantity(Rep raw) noexcept : raw_{raw} {}

    Rep raw_ = 0;
};

struct MoneyTag {};
struct WeightTag {};

using Money = Quantity<MoneyTag>;   // kopecks
using Weight = Quantity<WeightTag>; // grams

[[nodiscard]] constexpr Money kopecks(std::int64_t value) noexcept { return Money::fromRaw(value); }
[[nodiscard]] constexpr Money hryvnias(std::int64_t value) noexcept { return Money::fromRaw(value * 100); }
[[nodiscard]] constexpr Weight grams(std::int64_t value) noexcept { return Weight::fromRaw(value); }
[[nodiscard]] constexpr Weight kilograms(std::int64_t value) noexcept { return Weight::fromRaw(value * 1000); }

// Divides and rounds half away from zero (commercial rounding).
[[nodiscard]] constexpr std::int64_t divideRounded(std::int64_t numerator, std::int64_t denominator) noexcept {
    const std::int64_t half = denominator / 2;
    return (numerator >= 0 ? numerator + half : numerator - half) / denominator;
}

// Price of `weight` at `pricePerKg`, rounded to whole kopecks.
[[nodiscard]] constexpr Money costOf(Weight weight, Money pricePerKg) noexcept {
    // grams * kopecks/kg = kopecks * 1000
    return kopecks(divideRounded(weight.raw() * pricePerKg.raw(), 1000));
}

[[nodiscard]] std::string formatMoney(Money money);    // "1234.50 грн"
[[nodiscard]] std::string formatWeight(Weight weight); // "12.345 кг"

// Amount typed by the operator: "85", "85.5" or "85,50" (both separators, since
// the Ukrainian keyboard gives a comma). Empty for anything else, negatives included.
[[nodiscard]] std::optional<Money> parseMoney(std::string_view text);

} // namespace wms
