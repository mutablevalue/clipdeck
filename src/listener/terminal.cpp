#include "terminal.hpp"

#include <sys/select.h>
#include <unistd.h>

namespace clipdeck {

TerminalRawMode::TerminalRawMode(int file_descriptor)
    : file_descriptor_(file_descriptor) {
  if (!isatty(file_descriptor_)) {
    return;
  }

  termios current_state{};
  if (tcgetattr(file_descriptor_, &current_state) != 0) {
    return;
  }

  original_state_ = current_state;

  termios raw = current_state;
  raw.c_lflag &= static_cast<unsigned int>(~(ICANON | ECHO | ISIG));
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 0;

  if (tcsetattr(file_descriptor_, TCSANOW, &raw) == 0) {
    enabled_ = true;
  }
}

TerminalRawMode::~TerminalRawMode() {
  if (enabled_ && original_state_.has_value()) {
    tcsetattr(file_descriptor_, TCSANOW, &original_state_.value());
  }
}

bool TerminalRawMode::IsEnabled() const { return enabled_; }

bool InputAvailable(int file_descriptor, std::chrono::milliseconds timeout) {
  fd_set read_set;
  FD_ZERO(&read_set);
  FD_SET(file_descriptor, &read_set);

  timeval wait_time{};
  wait_time.tv_sec = static_cast<long>(timeout.count() / 1000);
  wait_time.tv_usec = static_cast<long>((timeout.count() % 1000) * 1000);

  const int ready =
      select(file_descriptor + 1, &read_set, nullptr, nullptr, &wait_time);

  return ready > 0 && FD_ISSET(file_descriptor, &read_set);
}

} // namespace clipdeck
