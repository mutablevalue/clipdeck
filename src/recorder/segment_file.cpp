#include "segment_file.hpp"

#include "../utils/logger.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <ranges>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>

namespace {

constexpr std::string_view kSegmentContext = "segment-file";

std::filesystem::file_time_type
SegmentModifiedTime(const std::filesystem::directory_entry &entry) {
  std::error_code error;
  const auto modified_time = entry.last_write_time(error);
  return error ? std::filesystem::file_time_type::min() : modified_time;
}

std::optional<std::string> RunFfprobeDuration(const std::filesystem::path &path) {
  std::array<int, 2> output_pipe{};
  if (pipe(output_pipe.data()) != 0) {
    return std::nullopt;
  }

  const pid_t pid = fork();
  if (pid < 0) {
    close(output_pipe[0]);
    close(output_pipe[1]);
    return std::nullopt;
  }

  if (pid == 0) {
    close(output_pipe[0]);
    dup2(output_pipe[1], STDOUT_FILENO);
    close(output_pipe[1]);

    std::array<std::string, 8> arguments{
        "ffprobe",
        "-v",
        "error",
        "-show_entries",
        "format=duration",
        "-of",
        "default=noprint_wrappers=1:nokey=1",
        path.string(),
    };

    std::array<char *, 9> argv{};
    for (std::size_t index = 0; index < arguments.size(); ++index) {
      argv[index] = arguments[index].data();
    }

    execvp("ffprobe", argv.data());
    std::_Exit(127);
  }

  close(output_pipe[1]);
  std::string output;
  std::array<char, 256> buffer{};
  while (true) {
    const ssize_t bytes_read =
        read(output_pipe[0], buffer.data(), buffer.size());
    if (bytes_read > 0) {
      output.append(buffer.data(), static_cast<std::size_t>(bytes_read));
      continue;
    }

    if (bytes_read == 0 || errno != EINTR) {
      break;
    }
  }
  close(output_pipe[0]);

  int status = 0;
  if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status) ||
      WEXITSTATUS(status) != 0) {
    return std::nullopt;
  }

  return output;
}

struct SegmentEntry {
  std::filesystem::path path;
  std::filesystem::file_time_type modified_time;
};

std::vector<SegmentEntry>
FinalizedSegmentEntries(const std::filesystem::path &segment_directory) {
  std::vector<SegmentEntry> segments;
  std::error_code error;

  if (!std::filesystem::exists(segment_directory, error)) {
    return segments;
  }

  for (const auto &entry :
       std::filesystem::directory_iterator(segment_directory, error)) {
    if (error || !entry.is_regular_file() || entry.path().extension() != ".mp4") {
      continue;
    }

    std::error_code size_error;
    if (entry.file_size(size_error) == 0 || size_error ||
        !clipdeck::IsFinalizedMp4Segment(entry.path())) {
      continue;
    }

    segments.push_back(SegmentEntry{.path = entry.path(),
                                    .modified_time = SegmentModifiedTime(entry)});
  }

  std::ranges::sort(segments, [](const auto &left, const auto &right) {
    if (left.modified_time != right.modified_time) {
      return left.modified_time < right.modified_time;
    }

    return left.path.filename().string() < right.path.filename().string();
  });

  return segments;
}

} // namespace

namespace clipdeck {

bool IsFinalizedMp4Segment(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  if (!input.is_open()) {
    return false;
  }

  std::string bytes((std::istreambuf_iterator<char>(input)),
                    std::istreambuf_iterator<char>());
  return bytes.find("moov") != std::string::npos;
}

std::optional<double> Mp4DurationSeconds(const std::filesystem::path &path) {
  const auto output = RunFfprobeDuration(path);
  if (!output.has_value()) {
    return std::nullopt;
  }

  const char *begin = output->c_str();
  char *end = nullptr;
  const double duration = std::strtod(begin, &end);
  if (begin == end || !std::isfinite(duration) || duration <= 0.0) {
    return std::nullopt;
  }

  return duration;
}

std::vector<std::filesystem::path> SelectRecentSegmentsForDuration(
    const std::filesystem::path &segment_directory,
    std::chrono::seconds target_duration) {
  const auto segments = FinalizedSegmentEntries(segment_directory);
  std::vector<std::filesystem::path> selected;
  double selected_duration = 0.0;
  const double target_seconds =
      static_cast<double>(std::max(target_duration.count(), 1L));

  for (const auto &segment : segments | std::views::reverse) {
    const auto duration = Mp4DurationSeconds(segment.path);
    if (!duration.has_value()) {
      Log(LogLevel::Debug, kSegmentContext,
          "Skipping segment without readable duration: " +
              segment.path.string());
      continue;
    }

    selected.push_back(segment.path);
    selected_duration += duration.value();

    if (selected_duration >= target_seconds) {
      break;
    }
  }

  std::ranges::reverse(selected);
  return selected;
}

std::size_t
FinalizedSegmentCount(const std::filesystem::path &segment_directory) {
  return FinalizedSegmentEntries(segment_directory).size();
}

} // namespace clipdeck
