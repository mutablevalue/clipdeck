#include "recorder_backend.hpp"

#include "recorder_setup.hpp"

#include <cstddef>

namespace clipdeck {

RecorderConfig BuildRecorderConfig(const ClipDeckSettings &settings) {
  const auto audio_source = ResolveCaptureAudioSource(settings);

  return RecorderConfig{.video_source = settings.capture_video_source,
                        .audio_enabled =
                            settings.capture_audio_enabled &&
                            audio_source.has_value(),
                        .audio_source =
                            audio_source.has_value() ? audio_source.value() : "",
                        .width = settings.capture_width,
                        .height = settings.capture_height,
                        .fps = settings.capture_fps,
                        .clip_length_seconds = settings.clip_length_seconds,
                        .buffer_safety_seconds = settings.buffer_safety_seconds,
                        .video_bitrate_kbps = settings.video_bitrate_kbps,
                        .audio_bitrate_kbps = settings.audio_bitrate_kbps,
                        .encoder = settings.encoder,
                        .clip_directory = settings.clip_directory};
}

std::size_t EstimateRecorderMemoryBudgetBytes(const RecorderConfig &config) {
  const auto buffer_seconds =
      static_cast<std::size_t>(config.clip_length_seconds +
                               config.buffer_safety_seconds);
  const auto total_kbps =
      static_cast<std::size_t>(config.video_bitrate_kbps +
                               (config.audio_enabled
                                    ? config.audio_bitrate_kbps
                                    : 0));
  const auto bytes = (total_kbps * 1000 * buffer_seconds) / 8;

  return bytes + (bytes / 4);
}

} // namespace clipdeck
