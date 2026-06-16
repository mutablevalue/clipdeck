#include "clip_muxer.hpp"

#include "../utils/logger.hpp"

#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>

namespace {

constexpr std::string_view kMuxerContext = "clip-muxer";

std::string TimestampForFilename() {
  const auto now = std::chrono::floor<std::chrono::seconds>(
      std::chrono::system_clock::now());
  return std::format("{:%Y%m%d-%H%M%S}", now);
}

} // namespace

namespace clipdeck {

ClipMuxer::ClipMuxer(std::filesystem::path clip_directory)
    : clip_directory_(std::move(clip_directory)) {}

std::optional<std::filesystem::path>
ClipMuxer::WriteClip(const std::vector<EncodedFragment> &fragments) const {
  if (fragments.empty()) {
    Log(LogLevel::Warning, kMuxerContext, "No encoded fragments are buffered.");
    return std::nullopt;
  }

  std::error_code error;
  std::filesystem::create_directories(clip_directory_, error);

  if (error) {
    Log(LogLevel::Error, kMuxerContext,
        "Failed to create clip directory: " + error.message());
    return std::nullopt;
  }

  const auto clip_path = BuildClipPath();
  std::ofstream output(clip_path, std::ios::binary | std::ios::trunc);

  if (!output.is_open()) {
    Log(LogLevel::Error, kMuxerContext,
        "Failed to open clip file: " + clip_path.string());
    return std::nullopt;
  }

  for (const auto &fragment : fragments) {
    output.write(reinterpret_cast<const char *>(fragment.bytes.data()),
                 static_cast<std::streamsize>(fragment.bytes.size()));
  }

  if (!output.good()) {
    Log(LogLevel::Error, kMuxerContext,
        "Failed while writing clip file: " + clip_path.string());
    return std::nullopt;
  }

  return clip_path;
}

std::filesystem::path ClipMuxer::BuildClipPath() const {
  return clip_directory_ / ("clipdeck-" + TimestampForFilename() + ".mp4");
}

} // namespace clipdeck
