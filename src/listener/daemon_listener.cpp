#include "daemon_listener.hpp"

#include "keybind.hpp"
#include "../utils/file_descriptor.hpp"
#include "../utils/logger.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <linux/input.h>
#include <poll.h>
#include <ranges>
#include <unistd.h>
#include <utility>
#include <vector>

namespace {

constexpr std::string_view kListenerContext = "daemon-listener";
constexpr auto kDeviceRetryInterval = std::chrono::seconds(5);

} // namespace

namespace clipdeck {

DaemonListener::DaemonListener(ListenerConfig config)
    : config_(std::move(config)) {}

DaemonListener::~DaemonListener() { Stop(); }

void DaemonListener::Start() {
  if (running_) {
    Log(LogLevel::Warning, kListenerContext,
        "Daemon listener is already running.");
    return;
  }

  running_ = true;
  listener_thread_ =
      std::jthread([this](std::stop_token stop_token) { ListenLoop(stop_token); });

  Log(LogLevel::Info, kListenerContext,
      "Started background keybind listener. Save keybind: " +
          config_.save_keybind + ".");
}

void DaemonListener::Stop() {
  if (!running_) {
    return;
  }

  running_ = false;

  if (listener_thread_.joinable()) {
    listener_thread_.request_stop();
    listener_thread_.join();
  }

  Log(LogLevel::Info, kListenerContext, "Stopped keybind listener.");
}

bool DaemonListener::IsRunning() const { return running_; }

void DaemonListener::SetKeybindCallback(KeybindCallback callback) {
  keybind_callback_ = std::move(callback);
}

void DaemonListener::ListenLoop(std::stop_token stop_token) {
  auto next_device_retry = std::chrono::steady_clock::now();

  while (running_ && !stop_token.stop_requested()) {
    if (input_devices_.empty() &&
        std::chrono::steady_clock::now() >= next_device_retry) {
      OpenInputDevices();
      next_device_retry = std::chrono::steady_clock::now() + kDeviceRetryInterval;
    }

    if (input_devices_.empty()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(250));
      continue;
    }

    std::vector<pollfd> poll_descriptors;
    poll_descriptors.reserve(input_devices_.size());

    for (const auto &device : input_devices_) {
      poll_descriptors.push_back({device.Get(), POLLIN, 0});
    }

    const int ready = poll(poll_descriptors.data(), poll_descriptors.size(), 250);
    if (ready <= 0) {
      continue;
    }

    for (const auto &descriptor : poll_descriptors) {
      if ((descriptor.revents & POLLIN) == 0) {
        continue;
      }

      input_event event{};
      const ssize_t bytes_read = read(descriptor.fd, &event, sizeof(event));

      if (bytes_read == sizeof(event)) {
        HandleInputEvent(event.type, event.code, event.value);
      } else if (bytes_read < 0 && errno != EAGAIN && errno != EWOULDBLOCK &&
                 errno != EINTR) {
        ResetKeyState();
      }
    }
  }

  running_ = false;
}

void DaemonListener::OpenInputDevices() {
  input_devices_.clear();
  ResetKeyState();
  int failed_devices = 0;
  std::string last_error;

  std::error_code error;
  if (!std::filesystem::exists(config_.input_directory, error)) {
    Log(LogLevel::Warning, kListenerContext,
        "Input directory does not exist: " + config_.input_directory.string());
    return;
  }

  for (const auto &entry :
       std::filesystem::directory_iterator(config_.input_directory, error)) {
    if (error) {
      break;
    }

    const std::string filename = entry.path().filename().string();
    if (!filename.starts_with("event")) {
      continue;
    }

    const int descriptor = open(entry.path().c_str(), O_RDONLY | O_NONBLOCK);
    if (descriptor >= 0) {
      input_devices_.emplace_back(descriptor);
      Log(LogLevel::Debug, kListenerContext,
          "Opened input device: " + entry.path().string() + ".");
      continue;
    }

    ++failed_devices;
    last_error = entry.path().string() + ": " + std::strerror(errno);
  }

  if (input_devices_.empty()) {
    Log(LogLevel::Warning, kListenerContext,
        "No readable input event devices found. Add the user to the input group or run with suitable permissions. Last error: " +
            (last_error.empty() ? std::string("none") : last_error));
    return;
  }

  Log(LogLevel::Info, kListenerContext,
      "Listening on " + std::to_string(input_devices_.size()) +
          " input event devices. Failed to open " +
          std::to_string(failed_devices) + " devices.");
}

void DaemonListener::HandleInputEvent(int event_type, int event_code,
                                      int event_value) {
  if (event_type != EV_KEY) {
    return;
  }

  if (event_value == 0) {
    std::erase(pressed_key_codes_, event_code);
    save_keybind_armed_ = true;
    return;
  }

  if (event_value == 1 &&
      std::ranges::find(pressed_key_codes_, event_code) ==
          pressed_key_codes_.end()) {
    pressed_key_codes_.push_back(event_code);
  }

  if (save_keybind_armed_ && SaveKeybindIsPressed()) {
    save_keybind_armed_ = false;
    Log(LogLevel::Info, kListenerContext, "Save keybind detected.");

    if (keybind_callback_) {
      keybind_callback_("save");
    }
  }
}

void DaemonListener::ResetKeyState() {
  pressed_key_codes_.clear();
  save_keybind_armed_ = true;
}

bool DaemonListener::SaveKeybindIsPressed() const {
  const auto requirements = ParseKeybindRequirements(config_.save_keybind);

  if (!requirements.has_value()) {
    return false;
  }

  return std::ranges::all_of(requirements.value(), [this](const auto &requirement) {
    return std::ranges::any_of(requirement.alternatives, [this](int key_code) {
      return std::ranges::find(pressed_key_codes_, key_code) !=
             pressed_key_codes_.end();
    });
  });
}

} // namespace clipdeck
