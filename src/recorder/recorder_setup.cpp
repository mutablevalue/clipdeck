#include "recorder_setup.hpp"

#include "../utils/logger.hpp"
#include "../utils/string_utils.hpp"

#include <array>
#include <cstdio>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#if defined(CLIPDECK_HAS_GSTREAMER)
#include <gst/gst.h>
#endif

namespace {

constexpr std::string_view kSetupContext = "setup";

std::optional<std::string> CaptureCommandOutput(const std::string &command) {
  std::array<char, 256> buffer{};
  std::string output;

  struct PipeCloser {
    void operator()(FILE *file) const {
      if (file != nullptr) {
        pclose(file);
      }
    }
  };

  std::unique_ptr<FILE, PipeCloser> pipe(popen(command.c_str(), "r"));
  if (pipe == nullptr) {
    return std::nullopt;
  }

  while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) !=
         nullptr) {
    output += buffer.data();
  }

  while (!output.empty() &&
         (output.back() == '\n' || output.back() == '\r' ||
          output.back() == ' ' || output.back() == '\t')) {
    output.pop_back();
  }

  if (output.empty()) {
    return std::nullopt;
  }

  return output;
}

std::optional<std::string> DetectPulseMonitorSource() {
  if (const auto default_sink = CaptureCommandOutput("pactl get-default-sink");
      default_sink.has_value()) {
    return default_sink.value() + ".monitor";
  }

  return CaptureCommandOutput(
      "pactl list short sources | awk '$2 ~ /\\.monitor$/ { print $2; exit }'");
}

#if defined(CLIPDECK_HAS_GSTREAMER)
std::vector<std::string>
RequiredElements(const clipdeck::ClipDeckSettings &settings) {
  std::vector<std::string> elements{"pipewiresrc", "videoconvert", "videoscale",
                                    "h264parse", "mp4mux", "appsink"};

  elements.push_back(settings.encoder == "x264" ? "x264enc" : "openh264enc");

  if (!settings.capture_audio_source.empty()) {
    elements.emplace_back("pulsesrc");
    elements.emplace_back("audioconvert");
    elements.emplace_back("audioresample");
    elements.emplace_back("avenc_aac");
    elements.emplace_back("aacparse");
  }

  return elements;
}
#endif

bool ValidateGStreamerPlugins(const clipdeck::ClipDeckSettings &settings) {
#if !defined(CLIPDECK_HAS_GSTREAMER)
  (void)settings;
  Log(LogLevel::Error, kSetupContext,
      "This build was compiled without GStreamer support.");
  return false;
#else
  GError *error = nullptr;
  if (!gst_init_check(nullptr, nullptr, &error)) {
    const std::string message =
        error == nullptr ? "unknown error" : error->message;
    if (error != nullptr) {
      g_error_free(error);
    }

    Log(LogLevel::Error, kSetupContext,
        "Failed to initialize GStreamer: " + message);
    return false;
  }

  bool ok = true;
  GstRegistry *registry = gst_registry_get();

  for (const auto &element : RequiredElements(settings)) {
    GstPluginFeature *feature =
        gst_registry_lookup_feature(registry, element.c_str());
    if (feature == nullptr) {
      Log(LogLevel::Error, kSetupContext,
          "Missing GStreamer element: " + element + ".");
      ok = false;
      continue;
    }

    gst_object_unref(feature);
    Log(LogLevel::Info, kSetupContext,
        "Found GStreamer element: " + element + ".");
  }

  return ok;
#endif
}

} // namespace

namespace clipdeck {

bool SetupNativeRecorder(ClipDeckSettings &settings) {
  if (settings.capture_video_source.empty()) {
    settings.capture_video_source = "portal";
  }

  Log(LogLevel::Info, kSetupContext,
      "Video source: " + settings.capture_video_source + ".");

  if (settings.capture_audio_source.empty()) {
    if (const auto monitor = DetectPulseMonitorSource(); monitor.has_value()) {
      settings.capture_audio_source = monitor.value();
    }
  }

  if (settings.capture_audio_source.empty()) {
    Log(LogLevel::Warning, kSetupContext,
        "No desktop audio monitor source was detected. Clips will be video-only until capture_audio_source is set.");
  } else {
    Log(LogLevel::Info, kSetupContext,
        "Audio source: " + settings.capture_audio_source + ".");
  }

  const bool plugins_ok = ValidateGStreamerPlugins(settings);
  if (!plugins_ok) {
    Log(LogLevel::Error, kSetupContext,
        "Install the missing GStreamer/PipeWire runtime plugins and rerun clipdeck setup.");
    return false;
  }

  Log(LogLevel::Info, kSetupContext,
      "Native recorder setup completed. Portal permission may still be requested when capture starts.");
  return true;
}

bool DiagnoseNativeRecorder(const ClipDeckSettings &settings) {
  bool ok = true;

  Log(LogLevel::Info, kSetupContext,
      "Capture: " + std::to_string(settings.capture_width) + "x" +
          std::to_string(settings.capture_height) + "@" +
          std::to_string(settings.capture_fps) + "fps.");
  Log(LogLevel::Info, kSetupContext,
      "Video source: " + settings.capture_video_source + ".");
  Log(LogLevel::Info, kSetupContext,
      "Audio source: " +
          (settings.capture_audio_source.empty()
               ? std::string("<none>")
               : settings.capture_audio_source) +
          ".");

  ok = ValidateGStreamerPlugins(settings) && ok;

  return ok;
}

} // namespace clipdeck
