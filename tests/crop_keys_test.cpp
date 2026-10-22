#include "wms/crop_keys.h"

#include <gtest/gtest.h>

#include <stdexcept>

using namespace wms;

using Codes = std::vector<std::string>;

TEST(CropKeys, NeedsAtLeastOneKey) {
    EXPECT_THROW(CropKeys{0}, std::invalid_argument);
}

TEST(CropKeys, FillTakesTheFirstCropsOfTheCatalogue) {
    CropKeys keys{3};
    keys.fill({"raspberry", "strawberry", "blueberry", "currant"});
    EXPECT_EQ(keys.keys(), (Codes{"raspberry", "strawberry", "blueberry"}));
}

TEST(CropKeys, FillDropsCropsGoneFromTheCatalogue) {
    CropKeys keys{3, {"raspberry", "gone", "blueberry"}};
    keys.fill({"raspberry", "blueberry", "currant"});
    EXPECT_EQ(keys.keys(), (Codes{"raspberry", "blueberry", "currant"}));
}

TEST(CropKeys, UsingAKeyDoesNotMoveIt) {
    CropKeys keys{3, {"raspberry", "strawberry", "blueberry"}};
    keys.use("blueberry");
    EXPECT_EQ(keys.keys(), (Codes{"raspberry", "strawberry", "blueberry"}));
}

// The newcomer takes the place of the key used longest ago; the others stay put.
TEST(CropKeys, ACropFromTheRestReplacesTheKeyUsedLongestAgo) {
    CropKeys keys{3, {"raspberry", "strawberry", "blueberry"}};
    keys.use("blueberry");
    keys.use("raspberry");
    keys.use("strawberry");
    keys.use("currant");
    EXPECT_EQ(keys.keys(), (Codes{"raspberry", "strawberry", "currant"}));
}

// Never used counts as longest ago; of several such the rightmost goes.
TEST(CropKeys, AKeyNeverUsedGoesFirst) {
    CropKeys keys{3, {"raspberry", "strawberry", "blueberry"}};
    keys.use("raspberry");
    keys.use("currant");
    EXPECT_EQ(keys.keys(), (Codes{"raspberry", "strawberry", "currant"}));
    keys.use("gooseberry");
    EXPECT_EQ(keys.keys(), (Codes{"raspberry", "gooseberry", "currant"}));
}

TEST(CropKeys, AFreePlaceIsTakenWithoutReplacing) {
    CropKeys keys{3, {"raspberry"}};
    keys.use("currant");
    EXPECT_EQ(keys.keys(), (Codes{"raspberry", "currant"}));
}

// What the terminal saves and reads back between runs.
TEST(CropKeys, RecentIsMostRecentFirstAndRestores) {
    CropKeys keys{2, {"raspberry", "strawberry"}};
    keys.use("strawberry");
    keys.use("raspberry");
    EXPECT_EQ(keys.recent(), (Codes{"raspberry", "strawberry"}));

    CropKeys restored{2, keys.keys(), keys.recent()};
    restored.use("currant");
    EXPECT_EQ(restored.keys(), (Codes{"raspberry", "currant"}));
}
