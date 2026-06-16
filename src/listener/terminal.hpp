#pragma once

#include <chrono>
#include <optional>
#include <termios.h>

namespace clipdeck {

class TerminalRawMode {
public:
  explicit TerminalRawMode(int file_descriptor);
  ~TerminalRawMode();

  TerminalRawMode(const TerminalRawMode &) = delete;
  TerminalRawMode &operator=(const TerminalRawMode &) = delete;

  [[nodiscard]] bool IsEnabled() const;

private:
  int file_descriptor_ = -1;
  bool enabled_ = false;
  std::optional<termios> original_state_;
};

bool InputAvailable(int file_descriptor, std::chrono::milliseconds timeout);

} // namespace clipdeck
