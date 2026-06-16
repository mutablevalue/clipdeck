#include "../utils/string_utils.hpp"

#include <gtest/gtest.h>
#include <vector>

TEST(StringUtilsTest, SplitsShellWordsWithQuotes) {
  const std::vector<std::string> words =
      clipdeck::SplitShellWords(R"(obs --profile "Main Profile" --scene Game)");

  ASSERT_EQ(words.size(), 5);
  EXPECT_EQ(words[0], "obs");
  EXPECT_EQ(words[1], "--profile");
  EXPECT_EQ(words[2], "Main Profile");
  EXPECT_EQ(words[3], "--scene");
  EXPECT_EQ(words[4], "Game");
}

TEST(StringUtilsTest, SplitsEscapedSpaces) {
  const std::vector<std::string> words =
      clipdeck::SplitShellWords(R"(obs --profile Main\ Profile)");

  ASSERT_EQ(words.size(), 3);
  EXPECT_EQ(words[2], "Main Profile");
}

TEST(StringUtilsTest, JoinsCommandPreview) {
  EXPECT_EQ(clipdeck::JoinCommandPreview({"clipdeck", "start"}),
            "clipdeck start");
}
