#pragma once

#include <chrono>
#include <filesystem>
#include <optional>
#include <vector>

namespace clipdeck {

[[nodiscard]] bool IsFinalizedMp4Segment(const std::filesystem::path &path);

[[nodiscard]] std::optional<double>
Mp4DurationSeconds(const std::filesystem::path &path);

[[nodiscard]] std::vector<std::filesystem::path> SelectRecentSegmentsForDuration(
    const std::filesystem::path &segment_directory,
    std::chrono::seconds target_duration);

[[nodiscard]] std::size_t
FinalizedSegmentCount(const std::filesystem::path &segment_directory);

} // namespace clipdeck
