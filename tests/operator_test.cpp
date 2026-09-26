#include "wms/operator.h"

#include <gtest/gtest.h>

#include <stdexcept>

using namespace wms;

TEST(Operator, ConfirmsIdentityByPin) {
    const Operator op{1, "Оксана", "4821"};
    EXPECT_EQ(op.id(), 1);
    EXPECT_EQ(op.name(), "Оксана");
    EXPECT_TRUE(op.checkPin("4821"));
    EXPECT_FALSE(op.checkPin("1234"));
    EXPECT_FALSE(op.checkPin(""));
}

TEST(Operator, RejectsInvalidData) {
    EXPECT_THROW((Operator{0, "Оксана", "4821"}), std::invalid_argument);
    EXPECT_THROW((Operator{1, "", "4821"}), std::invalid_argument);
    EXPECT_THROW((Operator{1, "Оксана", "123"}), std::invalid_argument);     // too short
    EXPECT_THROW((Operator{1, "Оксана", "1234567"}), std::invalid_argument); // too long
    EXPECT_THROW((Operator{1, "Оксана", "12a4"}), std::invalid_argument);    // digits only
    EXPECT_NO_THROW((Operator{1, "Оксана", "123456"}));
}
