#pragma once

#include <chrono>
#include <string>

namespace clipdeck {

struct CommandResult {
  int exit_code = -1;
  bool timed_out = false;
};

CommandResult RunShellCommand(std::string command, std::chrono::seconds timeout);

} // namespace clipdeck
