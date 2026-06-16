#include "../utils/runtime_paths.hpp"

#include <cstdlib>
#include <filesystem>
#include <gtest/gtest.h>

TEST(RuntimePathsTest, UsesConfiguredOutputDirectory) {
  const auto output_directory =
      std::filesystem::temp_directory_path() / "clipdeck-runtime-path-test";
  setenv("CLIPDECK_OUTPUT_DIR", output_directory.c_str(), 1);

  EXPECT_EQ(clipdeck::OutputDirectory(), output_directory);
  EXPECT_EQ(clipdeck::SettingsPath(), output_directory / "settings.conf");
  EXPECT_EQ(clipdeck::RuntimeDirectory(), output_directory / "runtime");
  EXPECT_EQ(clipdeck::SegmentDirectory(),
            output_directory / "runtime" / "segments");

  unsetenv("CLIPDECK_OUTPUT_DIR");
}
