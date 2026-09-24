#include "wms/supplier.h"
#include "wms/text.h"

#include <gtest/gtest.h>

#include <stdexcept>

using namespace wms;

TEST(FoldCase, LowercasesUkrainianAndAscii) {
    EXPECT_EQ(foldCase("ШЕВЧЕНКО Taras"), "шевченко taras");
    EXPECT_EQ(foldCase("ЄІЇҐ"), "єіїґ");
    EXPECT_EQ(foldCase("вже мала 123"), "вже мала 123");
}

TEST(Supplier, MatchesByExactId) {
    const Supplier s{42, "Шевченко Тарас", "A"};
    EXPECT_TRUE(s.matches("42"));
    EXPECT_FALSE(s.matches("4"));
    EXPECT_FALSE(s.matches("420"));
}

TEST(Supplier, MatchesByNameIgnoringCase) {
    const Supplier s{42, "Шевченко Тарас", "A"};
    EXPECT_TRUE(s.matches("шевч"));
    EXPECT_TRUE(s.matches("ТАРАС"));
    EXPECT_FALSE(s.matches("Франко"));
    EXPECT_FALSE(s.matches(""));
}

TEST(Supplier, RejectsInvalidData) {
    EXPECT_THROW((Supplier{0, "Ім'я", "A"}), std::invalid_argument);
    EXPECT_THROW((Supplier{1, "", "A"}), std::invalid_argument);
}
