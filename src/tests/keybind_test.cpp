#include "../listener/keybind.hpp"

#include <gtest/gtest.h>
#include <linux/input-event-codes.h>

TEST(KeybindTest, NormalizesKeybindDisplay) {
  EXPECT_EQ(clipdeck::NormalizeKeybind(" control + z + p "), "Ctrl+Z+P");
  EXPECT_EQ(clipdeck::NormalizeKeybind("shift+alt+x"), "Shift+Alt+X");
}

TEST(KeybindTest, ParsesSupportedRequirements) {
  const auto requirements = clipdeck::ParseKeybindRequirements("Ctrl+Z+P");

  ASSERT_TRUE(requirements.has_value());
  ASSERT_EQ(requirements->size(), 3);
  EXPECT_EQ(requirements->at(0).label, "Ctrl");
  EXPECT_EQ(requirements->at(1).label, "Z");
  EXPECT_EQ(requirements->at(2).label, "P");
  EXPECT_EQ(requirements->at(1).alternatives.at(0), KEY_Z);
  EXPECT_EQ(requirements->at(2).alternatives.at(0), KEY_P);
}

TEST(KeybindTest, RejectsUnsupportedTokens) {
  EXPECT_FALSE(clipdeck::ParseKeybindRequirements("Ctrl+F13").has_value());
  EXPECT_FALSE(clipdeck::ParseKeybindRequirements("").has_value());
}

TEST(KeybindTest, ConvertsRawControlCharacters) {
  EXPECT_EQ(clipdeck::RawCharacterToKeyToken('\x1A'), "Ctrl+Z");
  EXPECT_EQ(clipdeck::RawCharacterToKeyToken('p'), "P");
  EXPECT_FALSE(clipdeck::RawCharacterToKeyToken('\x1B').has_value());
}
