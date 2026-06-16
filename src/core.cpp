#include "core.hpp"

#include "recorder/gstreamer_recorder.hpp"
#include "recorder/recorder_backend.hpp"
#include "recorder/recorder_setup.hpp"
#include "utils/logger.hpp"
#include "utils/process.hpp"
#include "utils/runtime_paths.hpp"

#include <chrono>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <unistd.h>

namespace {

constexpr std::string_view kCoreContext = "core";
volatile std::sig_atomic_t shutdown_requested = 0;
volatile std::sig_atomic_t save_requested = 0;

void HandleShutdownSignal(int) {
  shutdown_requested = 1;
}

void HandleSaveSignal(int) {
  save_requested = 1;
}

std::string RecorderStatusLine(const clipdeck::RecorderStatus &status) {
  return "backend=" + status.backend + "\n" +
         "running=" + (status.running ? std::string("true") : "false") + "\n" +
         "healthy=" + (status.healthy ? std::string("true") : "false") + "\n" +
         "message=" + status.message + "\n" +
         "buffered_milliseconds=" +
         std::to_string(status.buffered_duration.count()) + "\n" +
         "buffered_bytes=" + std::to_string(status.buffered_bytes) + "\n" +
         "memory_budget_bytes=" +
         std::to_string(status.memory_budget_bytes) + "\n";
}

void WriteRecorderStatusFile(const clipdeck::RecorderStatus &status) {
  std::error_code error;
  std::filesystem::create_directories(
      clipdeck::RecorderStatusPath().parent_path(), error);

  if (error) {
    return;
  }

  std::ofstream output(clipdeck::RecorderStatusPath(), std::ios::trunc);
  if (output.is_open()) {
    output << RecorderStatusLine(status);
  }
}

void PrintRecorderStatusFile() {
  std::ifstream input(clipdeck::RecorderStatusPath());

  if (!input.is_open()) {
    Log(LogLevel::Warning, kCoreContext,
        "Recorder runtime status is not available yet.");
    return;
  }

  std::string line;
  while (std::getline(input, line)) {
    if (!line.empty()) {
      Log(LogLevel::Info, kCoreContext, "Recorder " + line + ".");
    }
  }
}

} // namespace

namespace clipdeck {

ClipDeckCore::ClipDeckCore() : settings_(settings_store_.Load()) {}

void ClipDeckCore::Start() {
  const auto pid_file = PidFilePath();

  if (const auto existing_pid = ReadPidFile(pid_file);
      existing_pid.has_value() && IsProcessRunning(existing_pid.value())) {
    Log(LogLevel::Warning, kCoreContext,
        "ClipDeck listener is already running with pid " +
            std::to_string(existing_pid.value()) + ".");
    return;
  }

  std::error_code error;
  std::filesystem::create_directories(settings_.clip_directory, error);

  if (error) {
    Log(LogLevel::Error, kCoreContext,
        "Failed to create clip directory: " + error.message());
    return;
  }

  const pid_t pid = fork();

  if (pid < 0) {
    Log(LogLevel::Error, kCoreContext, "Failed to start background listener.");
    return;
  }

  if (pid > 0) {
    if (WritePidFile(pid_file, pid)) {
      Log(LogLevel::Info, kCoreContext,
          "Started ClipDeck listener in the background with pid " +
              std::to_string(pid) + ".");
    }
    return;
  }

  setsid();
  RedirectStandardStreams(DaemonLogPath());

  ClipDeckCore daemon_core;
  const int exit_code = daemon_core.RunListener();
  RemovePidFile(pid_file);
  std::_Exit(exit_code);
}

int ClipDeckCore::RunListener() {
  shutdown_requested = 0;
  save_requested = 0;
  std::signal(SIGINT, HandleShutdownSignal);
  std::signal(SIGTERM, HandleShutdownSignal);
  std::signal(SIGUSR1, HandleSaveSignal);

  DaemonListener listener(BuildListenerConfig());

  Log(LogLevel::Info, kCoreContext, "ClipDeck listener runtime started.");

  auto recorder =
      std::make_unique<GStreamerRecorder>(BuildRecorderConfig(settings_));
  if (!recorder->Start()) {
    Log(LogLevel::Warning, kCoreContext,
        "Native recorder did not start. Keybinds will still listen, but saves will fail until the recorder is healthy.");
  }
  WriteRecorderStatusFile(recorder->Status());

  listener.SetKeybindCallback([&](std::string_view action) {
    if (action != "save") {
      return;
    }

    recorder->SaveClip();
    WriteRecorderStatusFile(recorder->Status());
  });

  listener.Start();

  auto next_status_write = std::chrono::steady_clock::now();

  while (listener.IsRunning() && shutdown_requested == 0) {
    if (save_requested != 0) {
      save_requested = 0;

      recorder->SaveClip();
      WriteRecorderStatusFile(recorder->Status());
    }

    if (std::chrono::steady_clock::now() >= next_status_write) {
      WriteRecorderStatusFile(recorder->Status());
      next_status_write = std::chrono::steady_clock::now() +
                          std::chrono::seconds(1);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(250));
  }

  listener.Stop();
  recorder->Stop();
  WriteRecorderStatusFile(recorder->Status());

  Log(LogLevel::Info, kCoreContext, "ClipDeck listener runtime stopped.");
  return 0;
}

void ClipDeckCore::Restart() {
  Stop();
  Start();
}

void ClipDeckCore::Stop() {
  const auto pid_file = PidFilePath();
  const auto pid = ReadPidFile(pid_file);

  if (!pid.has_value()) {
    Log(LogLevel::Info, kCoreContext, "ClipDeck listener is not running.");
    return;
  }

  if (!IsProcessRunning(pid.value())) {
    RemovePidFile(pid_file);
    Log(LogLevel::Warning, kCoreContext,
        "Removed stale ClipDeck pid file for pid " + std::to_string(pid.value()) +
            ".");
    return;
  }

  if (!RequestProcessStop(pid.value())) {
    Log(LogLevel::Error, kCoreContext,
        "Failed to stop ClipDeck listener with pid " +
            std::to_string(pid.value()) + ".");
    return;
  }

  for (int attempt = 0; attempt < 20; ++attempt) {
    if (!IsProcessRunning(pid.value())) {
      RemovePidFile(pid_file);
      Log(LogLevel::Info, kCoreContext,
          "Stopped ClipDeck listener pid " + std::to_string(pid.value()) + ".");
      return;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  Log(LogLevel::Info, kCoreContext,
      "Stop requested for ClipDeck listener pid " + std::to_string(pid.value()) +
          "; process is still shutting down.");
}

bool ClipDeckCore::Save() {
  Log(LogLevel::Info, kCoreContext, "Clip save requested.");

  const auto pid = ReadPidFile(PidFilePath());
  if (!pid.has_value() || !IsProcessRunning(pid.value())) {
    Log(LogLevel::Error, kCoreContext,
        "ClipDeck daemon is not running. Start it before saving from the native memory buffer.");
    return false;
  }

  if (kill(pid.value(), SIGUSR1) != 0) {
    Log(LogLevel::Error, kCoreContext,
        "Failed to request native clip save from daemon pid " +
            std::to_string(pid.value()) + ".");
    return false;
  }

  Log(LogLevel::Info, kCoreContext,
      "Requested native clip save from daemon pid " +
          std::to_string(pid.value()) + ".");
  return true;
}

void ClipDeckCore::Status() {
  const auto pid = ReadPidFile(PidFilePath());
  const bool running = pid.has_value() && IsProcessRunning(pid.value());

  if (pid.has_value() && !running) {
    RemovePidFile(PidFilePath());
    Log(LogLevel::Warning, kCoreContext,
        "Removed stale ClipDeck pid file for pid " + std::to_string(pid.value()) +
            ".");
  }

  const std::string status = running ? "running" : "stopped";

  Log(LogLevel::Info, kCoreContext, "ClipDeck listener status: " + status + ".");
  ShowSettings();

  if (running) {
    PrintRecorderStatusFile();
  } else {
    auto recorder =
        GStreamerRecorder(BuildRecorderConfig(settings_));
    const auto recorder_status = recorder.Status();
    Log(LogLevel::Info, kCoreContext,
        "Recorder backend: " + recorder_status.backend + ".");
    Log(LogLevel::Info, kCoreContext,
        "Recorder status: stopped.");
  }
}

bool ClipDeckCore::Setup() {
  if (!SetupNativeRecorder(settings_)) {
    return false;
  }

  if (SaveSettings()) {
    Log(LogLevel::Info, kCoreContext, "Saved native recorder settings.");
    return true;
  }

  return false;
}

bool ClipDeckCore::Diagnose() {
  const bool ok = DiagnoseNativeRecorder(settings_);

  if (ok) {
    Log(LogLevel::Info, kCoreContext, "Diagnostics completed without errors.");
  }

  return ok;
}

void ClipDeckCore::ShowSettings() const {
  Log(LogLevel::Info, kCoreContext,
      "Clip length: " + std::to_string(settings_.clip_length_seconds) +
          " seconds.");
  Log(LogLevel::Info, kCoreContext,
      "Buffer safety: " + std::to_string(settings_.buffer_safety_seconds) +
          " seconds.");
  Log(LogLevel::Info, kCoreContext, "Save keybind: " + settings_.save_keybind + ".");
  Log(LogLevel::Info, kCoreContext,
      "Clip directory: " + settings_.clip_directory.string() + ".");
  Log(LogLevel::Info, kCoreContext,
      "Capture video source: " + settings_.capture_video_source + ".");
  Log(LogLevel::Info, kCoreContext,
      "Capture audio source: " +
          (settings_.capture_audio_source.empty()
               ? std::string("<none>")
               : settings_.capture_audio_source) +
          ".");
  Log(LogLevel::Info, kCoreContext,
      "Capture size: " + std::to_string(settings_.capture_width) + "x" +
          std::to_string(settings_.capture_height) + ".");
  Log(LogLevel::Info, kCoreContext,
      "Capture FPS: " + std::to_string(settings_.capture_fps) + ".");
  Log(LogLevel::Info, kCoreContext,
      "Video bitrate: " + std::to_string(settings_.video_bitrate_kbps) +
          " kbps.");
  Log(LogLevel::Info, kCoreContext,
      "Audio bitrate: " + std::to_string(settings_.audio_bitrate_kbps) +
          " kbps.");
  Log(LogLevel::Info, kCoreContext, "Encoder: " + settings_.encoder + ".");
}

void ClipDeckCore::SetClipLength(int seconds) {
  settings_.clip_length_seconds = seconds;

  if (SaveSettings()) {
    Log(LogLevel::Info, kCoreContext,
        "Clip length set to " + std::to_string(seconds) + " seconds.");
  }
}

void ClipDeckCore::SetClipDirectory(
    const std::filesystem::path &clip_directory) {
  settings_.clip_directory = clip_directory;

  std::error_code error;
  std::filesystem::create_directories(settings_.clip_directory, error);

  if (error) {
    Log(LogLevel::Error, kCoreContext,
        "Failed to create clip directory: " + error.message());
    return;
  }

  if (SaveSettings()) {
    Log(LogLevel::Info, kCoreContext,
        "Clip directory set to " + settings_.clip_directory.string() + ".");
  }
}

void ClipDeckCore::SetSaveKeybind(std::string save_keybind) {
  settings_.save_keybind = std::move(save_keybind);

  if (SaveSettings()) {
    Log(LogLevel::Info, kCoreContext,
        "Save keybind set to " + settings_.save_keybind + ".");
  }
}

void ClipDeckCore::SetBufferSafety(int seconds) {
  settings_.buffer_safety_seconds = seconds;

  if (SaveSettings()) {
    Log(LogLevel::Info, kCoreContext,
        "Buffer safety set to " + std::to_string(seconds) + " seconds.");
  }
}

void ClipDeckCore::SetCaptureVideoSource(std::string source) {
  settings_.capture_video_source = std::move(source);

  if (SaveSettings()) {
    Log(LogLevel::Info, kCoreContext,
        "Capture video source set to " + settings_.capture_video_source + ".");
  }
}

void ClipDeckCore::SetCaptureAudioSource(std::string source) {
  settings_.capture_audio_source = std::move(source);

  if (SaveSettings()) {
    Log(LogLevel::Info, kCoreContext,
        "Capture audio source set to " +
            (settings_.capture_audio_source.empty()
                 ? std::string("<none>")
                 : settings_.capture_audio_source) +
            ".");
  }
}

void ClipDeckCore::SetCaptureSize(int width, int height) {
  settings_.capture_width = width;
  settings_.capture_height = height;

  if (SaveSettings()) {
    Log(LogLevel::Info, kCoreContext,
        "Capture size set to " + std::to_string(width) + "x" +
            std::to_string(height) + ".");
  }
}

void ClipDeckCore::SetCaptureFps(int fps) {
  settings_.capture_fps = fps;

  if (SaveSettings()) {
    Log(LogLevel::Info, kCoreContext,
        "Capture FPS set to " + std::to_string(fps) + ".");
  }
}

void ClipDeckCore::SetVideoBitrate(int bitrate_kbps) {
  settings_.video_bitrate_kbps = bitrate_kbps;

  if (SaveSettings()) {
    Log(LogLevel::Info, kCoreContext,
        "Video bitrate set to " + std::to_string(bitrate_kbps) + " kbps.");
  }
}

void ClipDeckCore::SetAudioBitrate(int bitrate_kbps) {
  settings_.audio_bitrate_kbps = bitrate_kbps;

  if (SaveSettings()) {
    Log(LogLevel::Info, kCoreContext,
        "Audio bitrate set to " + std::to_string(bitrate_kbps) + " kbps.");
  }
}

void ClipDeckCore::SetEncoder(std::string encoder) {
  if (encoder != "openh264" && encoder != "x264") {
    Log(LogLevel::Error, kCoreContext,
        "Encoder must be openh264 or x264.");
    return;
  }

  settings_.encoder = std::move(encoder);

  if (SaveSettings()) {
    Log(LogLevel::Info, kCoreContext,
        "Encoder set to " + settings_.encoder + ".");
  }
}

bool ClipDeckCore::IsRunning() const {
  const auto pid = ReadPidFile(PidFilePath());
  return pid.has_value() && IsProcessRunning(pid.value());
}

ListenerConfig ClipDeckCore::BuildListenerConfig() const {
  return ListenerConfig{settings_.save_keybind};
}

bool ClipDeckCore::SaveSettings() const {
  if (!settings_store_.Save(settings_)) {
    Log(LogLevel::Error, kCoreContext, "Failed to save settings.");
    return false;
  }

  return true;
}

} // namespace clipdeck
