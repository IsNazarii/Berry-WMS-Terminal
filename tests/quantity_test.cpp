#include "wms/quantity.h"

#include <gtest/gtest.h>

#include <functional>
#include <type_traits>

using namespace wms;

// Different units must not mix: this is the whole point of Quantity<Tag>.
static_assert(!std::is_invocable_v<std::plus<>, Money, Weight>);
static_assert(!std::is_convertible_v<Weight, Money>);
static_assert(!std::is_convertible_v<std::int64_t, Money>);

TEST(Quantity, Arithmetic) {
    EXPECT_EQ(hryvnias(3) + kopecks(50), kopecks(350));
    EXPECT_EQ(kilograms(2) - grams(1), grams(1'999));
    EXPECT_EQ(grams(350) * 10, kilograms(3) + grams(500));
    EXPECT_EQ(-kopecks(5), kopecks(-5));
}

TEST(Quantity, Comparison) {
    EXPECT_LT(grams(999), kilograms(1));
    EXPECT_TRUE(kopecks(1).isPositive());
    EXPECT_TRUE(kopecks(-1).isNegative());
    EXPECT_TRUE(Money{}.isZero());
}

TEST(Quantity, CostOfExactValue) {
    // 1.234 kg * 85.00 UAH/kg = 104.89 UAH
    EXPECT_EQ(costOf(grams(1'234), hryvnias(85)), kopecks(10'489));
}

TEST(Quantity, CostOfRoundsHalfAwayFromZero) {
    EXPECT_EQ(costOf(grams(9), kopecks(50)), kopecks(0));   // 0.45 kop
    EXPECT_EQ(costOf(grams(10), kopecks(50)), kopecks(1));  // 0.50 kop
    EXPECT_EQ(costOf(grams(-10), kopecks(50)), kopecks(-1));
}

TEST(Quantity, Formatting) {
    EXPECT_EQ(formatMoney(kopecks(123'450)), "1234.50 грн");
    EXPECT_EQ(formatMoney(kopecks(-5)), "-0.05 грн");
    EXPECT_EQ(formatWeight(grams(12'345)), "12.345 кг");
    EXPECT_EQ(formatWeight(grams(7)), "0.007 кг");
}

TEST(Quantity, ParseMoneyAcceptsCommaOrDot) {
    EXPECT_EQ(parseMoney("85"), hryvnias(85));
    EXPECT_EQ(parseMoney("85.5"), kopecks(8'550));
    EXPECT_EQ(parseMoney("85,05"), kopecks(8'505));
    EXPECT_EQ(parseMoney(" 0.99 "), kopecks(99));
}

TEST(Quantity, ParseMoneyRejectsWhatIsNotAnAmount) {
    EXPECT_FALSE(parseMoney(""));
    EXPECT_FALSE(parseMoney("  "));
    EXPECT_FALSE(parseMoney("-5"));
    EXPECT_FALSE(parseMoney("85.555")); // no fractions of a kopeck
    EXPECT_FALSE(parseMoney("85."));
    EXPECT_FALSE(parseMoney(".5"));
    EXPECT_FALSE(parseMoney("8a5"));
    EXPECT_FALSE(parseMoney("1.2.3"));
    EXPECT_FALSE(parseMoney("99999999999999999999")); // would overflow int64
}
