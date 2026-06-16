#include "../utils/number_parser.hpp"

#include <gtest/gtest.h>

TEST(NumberParserTest, ParsesPositiveIntegers) {
  int value = 0;

  EXPECT_TRUE(clipdeck::ParsePositiveInteger("30", value));
  EXPECT_EQ(value, 30);
}

TEST(NumberParserTest, RejectsInvalidIntegers) {
  int value = 0;

  EXPECT_FALSE(clipdeck::ParsePositiveInteger("0", value));
  EXPECT_FALSE(clipdeck::ParsePositiveInteger("-1", value));
  EXPECT_FALSE(clipdeck::ParsePositiveInteger("12s", value));
  EXPECT_FALSE(clipdeck::ParsePositiveInteger("", value));
}
