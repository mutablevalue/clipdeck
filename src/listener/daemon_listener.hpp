#pragma once

#include "../utils/file_descriptor.hpp"

#include <atomic>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace clipdeck {

struct ListenerConfig {
  std::string save_keybind = "Ctrl+Z+P";
  std::filesystem::path input_directory = "/dev/input";
};

class DaemonListener {
public:
  using KeybindCallback = std::function<void(std::string_view action)>;

  explicit DaemonListener(ListenerConfig config = {});
  ~DaemonListener();

  DaemonListener(const DaemonListener &) = delete;
  DaemonListener &operator=(const DaemonListener &) = delete;

  void Start();
  void Stop();
  [[nodiscard]] bool IsRunning() const;
  void SetKeybindCallback(KeybindCallback callback);

private:
  void ListenLoop(std::stop_token stop_token);
  void OpenInputDevices();
  void HandleInputEvent(int event_type, int event_code, int event_value);
  [[nodiscard]] bool SaveKeybindIsPressed() const;

  ListenerConfig config_;
  KeybindCallback keybind_callback_;
  std::atomic_bool running_{false};
  std::jthread listener_thread_;
  std::vector<FileDescriptor> input_devices_;
  std::vector<int> pressed_key_codes_;
  bool save_keybind_armed_ = true;
};

} // namespace clipdeck
