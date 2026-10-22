#include "wms/crop.h"

#include <gtest/gtest.h>

#include <stdexcept>

using namespace wms;

TEST(Crop, KeepsCodeAndName) {
    const Crop crop{"raspberry", "Малина"};
    EXPECT_EQ(crop.code(), "raspberry");
    EXPECT_EQ(crop.name(), "Малина");
}

TEST(Crop, RequiresCodeAndName) {
    EXPECT_THROW((Crop{"", "Малина"}), std::invalid_argument);
    EXPECT_THROW((Crop{"raspberry", ""}), std::invalid_argument);
}

TEST(NewCrop, AcceptsAFreshName) {
    const std::vector<Crop> existing{{"raspberry", "Малина"}};
    EXPECT_TRUE(validateNewCrop("Агрус", existing).empty());
}

TEST(NewCrop, RequiresName) {
    EXPECT_EQ(validateNewCrop("  ", {}).size(), 1u);
}

TEST(NewCrop, RejectsTheSameNameIgnoringCaseAndSpaces) {
    const std::vector<Crop> existing{{"raspberry", "Малина"}};
    EXPECT_EQ(validateNewCrop(" малина ", existing).size(), 1u);
    EXPECT_EQ(validateNewCrop("МАЛИНА", existing).size(), 1u);
}

// A code taken by an older crop would merge two crops in the journal.
TEST(NewCrop, RejectsANameThatFoldsToAnExistingCode) {
    const std::vector<Crop> existing{{"агрус", "Агрус зелений"}};
    EXPECT_EQ(validateNewCrop("Агрус", existing).size(), 1u);
}

TEST(NewCrop, CodeIsTheNormalisedName) {
    EXPECT_EQ(cropCodeFor("  Чорна   Смородина "), "чорна смородина");
}
