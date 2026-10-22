#include "wms/crop.h"
#include "wms/journal.h"

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

TEST(Crop, IsVisibleUnlessArchived) {
    EXPECT_FALSE((Crop{"raspberry", "Малина"}).archived());
    EXPECT_TRUE((Crop{"raspberry", "Малина", true}).archived());
}

// Adding the name of an archived crop brings that crop back, not a second one.
TEST(NewCrop, TheNameOfAnArchivedCropIsFree) {
    const std::vector<Crop> existing{{"raspberry", "Малина", true}};
    EXPECT_TRUE(validateNewCrop("малина", existing).empty());
    ASSERT_NE(findCropNamed("малина", existing), nullptr);
    EXPECT_EQ(findCropNamed("малина", existing)->code(), "raspberry");
    EXPECT_EQ(findCropNamed("Агрус", existing), nullptr);
}

TEST(RemoveCrop, AllowedWithAnEmptyWarehouse) {
    EXPECT_TRUE(validateRemoveCrop("raspberry", Journal{}).empty());
}

// The warehouse lists visible crops only: the boxes would vanish from the screen.
TEST(RemoveCrop, RefusedWhileTheCropIsInTheWarehouse) {
    Journal journal;
    const Receipt receipt = Receipt::create(42, Settlement::OnSite, {{
        .cropCode = "raspberry",
        .gross = kilograms(31),
        .boxCount = 10,
        .tarePerBox = grams(100),
        .pricePerKg = hryvnias(85),
    }});
    journal.add(receipt);
    journal.add(Allocation::wholeWeighing(receipt, 0, ToWarehouse{}));
    EXPECT_EQ(validateRemoveCrop("raspberry", journal).size(), 1u);
}
