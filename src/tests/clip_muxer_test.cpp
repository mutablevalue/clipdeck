#include "../recorder/clip_muxer.hpp"
#include "../recorder/segment_file.hpp"

#include <chrono>
#include <filesystem>
#include <gtest/gtest.h>
#include <string>
#include <unistd.h>

namespace {

std::filesystem::path TestDirectory(std::string_view name) {
  const auto suffix =
      std::chrono::steady_clock::now().time_since_epoch().count();
  return std::filesystem::temp_directory_path() /
         ("clipdeck-" + std::string(name) + "-" + std::to_string(getpid()) +
          "-" + std::to_string(suffix));
}

} // namespace

TEST(ClipMuxerTest, CreatesFullLengthBlackClipWhenNoSegmentsAreAvailable) {
  const auto root = TestDirectory("muxer-black");
  const auto clips = root / "clips";
  const auto temp = root / "runtime";
  std::filesystem::create_directories(clips);
  std::filesystem::create_directories(temp);

  clipdeck::ClipMuxer muxer(clips, temp);
  const auto clip = muxer.WriteClipFromSegments(
      {}, clipdeck::ClipMuxerOptions{.target_duration = std::chrono::seconds(2),
                                     .width = 320,
                                     .height = 180,
                                     .fps = 10,
                                     .video_bitrate_kbps = 500,
                                     .audio_bitrate_kbps = 96,
                                     .audio_enabled = true});

  ASSERT_TRUE(clip.has_value());
  EXPECT_TRUE(std::filesystem::exists(clip.value()));

  const auto duration = clipdeck::Mp4DurationSeconds(clip.value());
  ASSERT_TRUE(duration.has_value());
  EXPECT_NEAR(duration.value(), 2.0, 0.4);

  std::error_code error;
  EXPECT_TRUE(std::filesystem::is_empty(temp, error));
  std::filesystem::remove_all(root, error);
}
