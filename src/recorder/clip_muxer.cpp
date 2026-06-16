#include "clip_muxer.hpp"

#include "../utils/app_error.hpp"
#include "../utils/logger.hpp"
#include "../utils/runtime_paths.hpp"
#include "segment_file.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace {

constexpr std::string_view kMuxerContext = "clip-muxer";

std::string TimestampForFilename() {
  const auto now = std::chrono::floor<std::chrono::seconds>(
      std::chrono::system_clock::now());
  const auto milliseconds =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch()) %
      std::chrono::seconds(1);
  return std::format("{:%Y%m%d-%H%M%S}-{:03}", now, milliseconds.count());
}

std::string EscapeConcatPath(const std::filesystem::path &path) {
  std::string escaped;
  for (const char character : path.string()) {
    if (character == '\'') {
      escaped += "'\\''";
      continue;
    }

    escaped.push_back(character);
  }

  return escaped;
}

std::string FormatSeconds(double seconds) {
  std::ostringstream output;
  output << std::fixed << std::setprecision(3) << seconds;
  return output.str();
}

int RunCommand(std::vector<std::string> arguments) {
  if (arguments.empty()) {
    return -1;
  }

  const pid_t pid = fork();

  if (pid < 0) {
    return -1;
  }

  if (pid == 0) {
    std::vector<char *> argv;
    argv.reserve(arguments.size() + 1);
    for (auto &argument : arguments) {
      argv.push_back(argument.data());
    }
    argv.push_back(nullptr);

    execvp("ffmpeg", argv.data());
    std::_Exit(127);
  }

  int status = 0;
  if (waitpid(pid, &status, 0) != pid) {
    return -1;
  }

  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  }

  if (WIFSIGNALED(status)) {
    return 128 + WTERMSIG(status);
  }

  return -1;
}

std::string RunCommandCapture(std::vector<std::string> arguments) {
  if (arguments.empty()) {
    return {};
  }

  std::array<int, 2> output_pipe{};
  if (pipe(output_pipe.data()) != 0) {
    return {};
  }

  const pid_t pid = fork();
  if (pid < 0) {
    close(output_pipe[0]);
    close(output_pipe[1]);
    return {};
  }

  if (pid == 0) {
    close(output_pipe[0]);
    dup2(output_pipe[1], STDOUT_FILENO);
    dup2(output_pipe[1], STDERR_FILENO);
    close(output_pipe[1]);

    std::vector<char *> argv;
    argv.reserve(arguments.size() + 1);
    for (auto &argument : arguments) {
      argv.push_back(argument.data());
    }
    argv.push_back(nullptr);

    execvp(argv.front(), argv.data());
    std::_Exit(127);
  }

  close(output_pipe[1]);
  std::string output;
  std::array<char, 4096> buffer{};
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
  waitpid(pid, &status, 0);
  return output;
}

bool FfmpegEncoderAvailable(std::string_view encoder) {
  const auto output =
      RunCommandCapture({"ffmpeg", "-hide_banner", "-encoders"});
  return output.find(std::string(encoder)) != std::string::npos;
}

std::vector<std::string>
VideoEncoderArguments(const clipdeck::ClipMuxerOptions &options) {
  if (FfmpegEncoderAvailable("libx264")) {
    return {"-c:v", "libx264", "-preset", "veryfast", "-crf", "23"};
  }

  if (FfmpegEncoderAvailable("libopenh264")) {
    return {"-c:v", "libopenh264", "-b:v",
            std::to_string(options.video_bitrate_kbps) + "k"};
  }

  return {"-c:v", "mpeg4", "-q:v", "4"};
}

void AppendVideoEncoderArguments(std::vector<std::string> &arguments,
                                 const clipdeck::ClipMuxerOptions &options) {
  for (auto argument : VideoEncoderArguments(options)) {
    arguments.push_back(std::move(argument));
  }
  arguments.emplace_back("-pix_fmt");
  arguments.emplace_back("yuv420p");
}

void AppendAudioEncoderArguments(std::vector<std::string> &arguments,
                                 const clipdeck::ClipMuxerOptions &options) {
  arguments.emplace_back("-c:a");
  arguments.emplace_back("aac");
  arguments.emplace_back("-b:a");
  arguments.emplace_back(std::to_string(options.audio_bitrate_kbps) + "k");
}

bool CreateBlackPaddingSegment(const std::filesystem::path &path,
                               double duration_seconds,
                               const clipdeck::ClipMuxerOptions &options) {
  std::vector<std::string> arguments{
      "ffmpeg",
      "-hide_banner",
      "-loglevel",
      "error",
      "-y",
      "-f",
      "lavfi",
      "-i",
      std::format("color=c=black:s={}x{}:r={}:d={}", options.width,
                  options.height, options.fps,
                  FormatSeconds(duration_seconds)),
  };

  if (options.audio_enabled) {
    arguments.insert(arguments.end(),
                     {"-f", "lavfi", "-i",
                      "anullsrc=channel_layout=stereo:sample_rate=48000"});
  }

  arguments.insert(arguments.end(), {"-t", FormatSeconds(duration_seconds),
                                     "-map", "0:v:0"});

  if (options.audio_enabled) {
    arguments.insert(arguments.end(), {"-map", "1:a:0"});
  }

  AppendVideoEncoderArguments(arguments, options);
  if (options.audio_enabled) {
    AppendAudioEncoderArguments(arguments, options);
    arguments.emplace_back("-shortest");
  }

  arguments.insert(arguments.end(), {"-movflags", "+faststart", path.string()});

  return RunCommand(std::move(arguments)) == 0;
}

int RunFfmpegCompose(const std::filesystem::path &manifest_path,
                     const std::filesystem::path &output_path,
                     double trim_start_seconds,
                     const clipdeck::ClipMuxerOptions &options) {
  std::vector<std::string> arguments{
      "ffmpeg", "-hide_banner", "-loglevel", "error", "-y",
      "-f",     "concat",      "-safe",    "0",     "-i",
      manifest_path.string()};

  if (trim_start_seconds > 0.001) {
    arguments.insert(arguments.end(), {"-ss", FormatSeconds(trim_start_seconds)});
  }

  arguments.insert(arguments.end(),
                   {"-t", FormatSeconds(options.target_duration.count()), "-map",
                    "0:v:0", "-map", "0:a:0?"});

  AppendVideoEncoderArguments(arguments, options);
  if (options.audio_enabled) {
    AppendAudioEncoderArguments(arguments, options);
  } else {
    arguments.insert(arguments.end(), {"-an"});
  }

  arguments.insert(arguments.end(),
                   {"-movflags", "+faststart", output_path.string()});

  return RunCommand(std::move(arguments));
}

struct StagedSegments {
  std::vector<std::filesystem::path> paths;
  double duration_seconds = 0.0;
};

StagedSegments StageSegments(const std::vector<std::filesystem::path> &segments,
                             const std::filesystem::path &staging_directory) {
  StagedSegments staged;
  int index = 0;

  for (const auto &segment : segments) {
    const auto source_duration = clipdeck::Mp4DurationSeconds(segment);
    if (!source_duration.has_value()) {
      Log(LogLevel::Debug, kMuxerContext,
          "Skipping segment without readable duration before staging: " +
              segment.string());
      continue;
    }

    const auto staged_path =
        staging_directory / std::format("segment-{:05}.mp4", index++);
    std::error_code copy_error;
    std::filesystem::copy_file(segment, staged_path,
                               std::filesystem::copy_options::overwrite_existing,
                               copy_error);
    if (copy_error) {
      Log(LogLevel::Warning, kMuxerContext,
          "Failed to stage segment " + segment.string() + ": " +
              copy_error.message());
      continue;
    }

    const auto staged_duration = clipdeck::Mp4DurationSeconds(staged_path);
    if (!staged_duration.has_value()) {
      Log(LogLevel::Warning, kMuxerContext,
          "Staged segment is not readable: " + staged_path.string());
      std::filesystem::remove(staged_path, copy_error);
      continue;
    }

    staged.paths.push_back(staged_path);
    staged.duration_seconds += staged_duration.value();
  }

  return staged;
}

} // namespace

namespace clipdeck {

ClipMuxer::ClipMuxer(std::filesystem::path clip_directory)
    : ClipMuxer(std::move(clip_directory),
                RuntimeDirectory() / "clip-compose") {}

ClipMuxer::ClipMuxer(std::filesystem::path clip_directory,
                     std::filesystem::path temp_directory)
    : clip_directory_(std::move(clip_directory)),
      temp_directory_(std::move(temp_directory)) {}

std::optional<std::filesystem::path>
ClipMuxer::WriteClipFromSegments(
    const std::vector<std::filesystem::path> &segments) const {
  return WriteClipFromSegments(segments, ClipMuxerOptions{});
}

std::optional<std::filesystem::path>
ClipMuxer::WriteClipFromSegments(
    const std::vector<std::filesystem::path> &segments,
    const ClipMuxerOptions &options) const {
  const double target_seconds =
      static_cast<double>(std::max(options.target_duration.count(), 1L));

  std::error_code error;
  std::filesystem::create_directories(clip_directory_, error);

  if (error) {
    HandleError(MakeError("clip_directory", kMuxerContext,
                          "Failed to create clip directory: " +
                              error.message()));
    return std::nullopt;
  }

  std::filesystem::create_directories(temp_directory_, error);
  if (error) {
    HandleError(MakeError("clip_temp_directory", kMuxerContext,
                          "Failed to create clip temp directory: " +
                              error.message()));
    return std::nullopt;
  }

  const auto staging_directory = BuildStagingDirectory();
  std::filesystem::create_directories(staging_directory, error);
  if (error) {
    HandleError(MakeError("clip_staging_directory", kMuxerContext,
                          "Failed to create clip staging directory: " +
                              error.message()));
    return std::nullopt;
  }

  const auto cleanup_staging = [&] {
    std::error_code cleanup_error;
    std::filesystem::remove_all(staging_directory, cleanup_error);
  };

  auto staged = StageSegments(segments, staging_directory);
  if (staged.paths.empty()) {
    Log(LogLevel::Warning, kMuxerContext,
        "No finalized recorder segments were available; creating a black clip.");
  }

  const double padding_seconds =
      std::max(0.0, target_seconds - staged.duration_seconds);
  std::vector<std::filesystem::path> compose_paths;
  double compose_duration = staged.duration_seconds;

  if (padding_seconds > 0.001) {
    const auto padding_path = staging_directory / "black-padding.mp4";
    if (!CreateBlackPaddingSegment(padding_path, padding_seconds, options)) {
      HandleError(MakeError("clip_padding", kMuxerContext,
                            "ffmpeg failed to create black padding segment."));
      cleanup_staging();
      return std::nullopt;
    }

    compose_paths.push_back(padding_path);
    compose_duration += padding_seconds;
  }

  compose_paths.insert(compose_paths.end(), staged.paths.begin(),
                       staged.paths.end());

  const auto manifest_path = staging_directory / "concat.txt";
  const auto temp_output_path = staging_directory / "clipdeck-output.mp4";
  const auto clip_path = BuildClipPath();

  std::ofstream manifest(manifest_path, std::ios::trunc);
  if (!manifest.is_open()) {
    HandleError(MakeError("clip_manifest", kMuxerContext,
                          "Failed to open concat manifest: " +
                              manifest_path.string()));
    cleanup_staging();
    return std::nullopt;
  }

  for (const auto &segment : compose_paths) {
    manifest << "file '" << EscapeConcatPath(std::filesystem::absolute(segment))
             << "'\n";
  }
  manifest.close();

  const double trim_start_seconds =
      std::max(0.0, compose_duration - target_seconds);
  const int exit_code =
      RunFfmpegCompose(manifest_path, temp_output_path, trim_start_seconds,
                       options);

  if (exit_code != 0) {
    HandleError(MakeError("clip_compose", kMuxerContext,
                          "ffmpeg failed to compose clip, exit code " +
                              std::to_string(exit_code) + "."));
    cleanup_staging();
    return std::nullopt;
  }

  std::error_code status_error;
  if (!std::filesystem::exists(temp_output_path, status_error) ||
      std::filesystem::file_size(temp_output_path, status_error) == 0) {
    HandleError(MakeError("clip_empty", kMuxerContext,
                          "Composed clip is missing or empty: " +
                              temp_output_path.string()));
    cleanup_staging();
    return std::nullopt;
  }

  std::filesystem::rename(temp_output_path, clip_path, error);
  if (error) {
    error.clear();
    std::filesystem::copy_file(temp_output_path, clip_path,
                               std::filesystem::copy_options::overwrite_existing,
                               error);
    if (error) {
      HandleError(MakeError("clip_publish", kMuxerContext,
                            "Failed to publish final clip: " +
                                error.message()));
      cleanup_staging();
      return std::nullopt;
    }
    std::filesystem::remove(temp_output_path, status_error);
  }

  cleanup_staging();
  return clip_path;
}

std::filesystem::path ClipMuxer::BuildClipPath() const {
  return clip_directory_ / ("clipdeck-" + TimestampForFilename() + ".mp4");
}

std::filesystem::path ClipMuxer::BuildStagingDirectory() const {
  return temp_directory_ /
         ("clipdeck-compose-" + TimestampForFilename() + "-" +
          std::to_string(getpid()));
}

} // namespace clipdeck
