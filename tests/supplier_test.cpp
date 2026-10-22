#include "wms/supplier.h"
#include "wms/text.h"

#include <gtest/gtest.h>

#include <stdexcept>
#include <vector>

using namespace wms;

TEST(FoldCase, LowercasesUkrainianAndAscii) {
    EXPECT_EQ(foldCase("ШЕВЧЕНКО Taras"), "шевченко taras");
    EXPECT_EQ(foldCase("ЄІЇҐ"), "єіїґ");
    EXPECT_EQ(foldCase("вже мала 123"), "вже мала 123");
}

TEST(Supplier, MatchesByExactId) {
    const Supplier s{42, "Шевченко Тарас"};
    EXPECT_TRUE(s.matches("42"));
    EXPECT_FALSE(s.matches("4"));
    EXPECT_FALSE(s.matches("420"));
}

TEST(Supplier, MatchesByNameIgnoringCase) {
    const Supplier s{42, "Шевченко Тарас"};
    EXPECT_TRUE(s.matches("шевч"));
    EXPECT_TRUE(s.matches("ТАРАС"));
    EXPECT_FALSE(s.matches("Франко"));
    EXPECT_FALSE(s.matches(""));
}

TEST(Supplier, RejectsInvalidData) {
    EXPECT_THROW((Supplier{0, "Ім'я"}), std::invalid_argument);
    EXPECT_THROW((Supplier{1, ""}), std::invalid_argument);
}

TEST(SqueezeSpaces, TrimsAndCollapsesRuns) {
    EXPECT_EQ(squeezeSpaces("  Петренко   Іван \t"), "Петренко Іван");
    EXPECT_EQ(squeezeSpaces("   "), "");
    EXPECT_EQ(squeezeSpaces("Малина"), "Малина");
}

TEST(NewSupplier, AcceptsNameWithOptionalPhone) {
    const std::vector<Supplier> existing{{42, "Шевченко Тарас"}};
    EXPECT_TRUE(validateNewSupplier("Петренко Іван", "", existing).empty());
    EXPECT_TRUE(validateNewSupplier("Петренко Іван", "+38 (067) 111-22-33", existing).empty());
}

TEST(NewSupplier, RequiresName) {
    EXPECT_EQ(validateNewSupplier("   ", "", {}).size(), 1u);
}

TEST(NewSupplier, RejectsMalformedPhone) {
    EXPECT_EQ(validateNewSupplier("Петренко", "067-11", {}).size(), 1u);    // too few digits
    EXPECT_EQ(validateNewSupplier("Петренко", "0671112233a", {}).size(), 1u); // not a phone character
}

TEST(NewSupplier, RejectsTheSameNameIgnoringCaseAndSpaces) {
    const std::vector<Supplier> existing{{42, "Шевченко Тарас"}};
    EXPECT_EQ(validateNewSupplier(" шевченко  ТАРАС ", "", existing).size(), 1u);
    EXPECT_TRUE(validateNewSupplier("Шевченко Тарас, Лосятин", "", existing).empty());
}

TEST(NewSupplier, IdFollowsTheLargest) {
    EXPECT_EQ(nextSupplierId({}), 1);
    EXPECT_EQ(nextSupplierId({{42, "А"}, {7, "Б"}}), 43);
}
