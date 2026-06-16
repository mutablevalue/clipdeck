#pragma once

#include "../settings/settings_store.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace clipdeck {

struct AudioCaptureSource {
  std::string name;
  bool monitor = false;
  bool default_output_monitor = false;
};

[[nodiscard]] std::vector<AudioCaptureSource> ParseAudioCaptureSources(
    std::string_view pactl_sources_output,
    const std::optional<std::string> &default_output_monitor);
[[nodiscard]] std::optional<std::string> SelectAutomaticAudioMonitor(
    const std::vector<AudioCaptureSource> &sources);
[[nodiscard]] std::vector<AudioCaptureSource> AvailableAudioCaptureSources();
[[nodiscard]] std::optional<std::string>
ResolveCaptureAudioSource(const ClipDeckSettings &settings);
bool SetupNativeRecorder(ClipDeckSettings &settings);
bool DiagnoseNativeRecorder(const ClipDeckSettings &settings);

} // namespace clipdeck
