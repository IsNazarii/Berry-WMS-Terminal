#include "wms/uuid.h"

#include <gtest/gtest.h>

using namespace wms;
using namespace std::chrono;

TEST(Uuid, GeneratedIsVersion7WithRfcVariant) {
    const Uuid id = Uuid::generateV7();
    EXPECT_EQ(id.version(), 7);
    EXPECT_EQ(id.bytes()[8] & 0xC0, 0x80);
    EXPECT_FALSE(id.isNil());
}

TEST(Uuid, GeneratedIdsAreUnique) { EXPECT_NE(Uuid::generateV7(), Uuid::generateV7()); }

TEST(Uuid, EncodesTimestamp) {
    const system_clock::time_point at{milliseconds{1'790'000'000'123}};
    EXPECT_EQ(Uuid::generateV7(at, 0, 0).timestamp(), at);
}

TEST(Uuid, SortsByCreationTime) {
    const system_clock::time_point t{milliseconds{1'790'000'000'000}};
    const Uuid earlier = Uuid::generateV7(t, ~0ULL, ~0ULL);
    const Uuid later = Uuid::generateV7(t + 1ms, 0, 0);
    EXPECT_LT(earlier, later);
}

TEST(Uuid, StringRoundTrip) {
    const Uuid id = Uuid::generateV7();
    const std::string text = id.toString();
    ASSERT_EQ(text.size(), 36U);
    EXPECT_EQ(text[14], '7');
    EXPECT_EQ(Uuid::parse(text), id);
}

TEST(Uuid, ParseAcceptsUpperCase) {
    const auto id = Uuid::parse("0192F3A4-5B6C-7D8E-9F01-23456789ABCD");
    ASSERT_TRUE(id.has_value());
    EXPECT_EQ(id->toString(), "0192f3a4-5b6c-7d8e-9f01-23456789abcd");
}

TEST(Uuid, ParseRejectsMalformed) {
    EXPECT_FALSE(Uuid::parse(""));
    EXPECT_FALSE(Uuid::parse("0192f3a4-5b6c-7d8e-9f01-23456789abc"));   // too short
    EXPECT_FALSE(Uuid::parse("0192f3a4x5b6c-7d8e-9f01-23456789abcd"));  // bad separator
    EXPECT_FALSE(Uuid::parse("0192f3a4-5b6c-7d8e-9f01-23456789abcg"));  // bad hex
}

TEST(Uuid, DefaultIsNil) {
    EXPECT_TRUE(Uuid{}.isNil());
    EXPECT_EQ(Uuid{}.toString(), "00000000-0000-0000-0000-000000000000");
}
