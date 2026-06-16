#include "command_runner.hpp"

#include <csignal>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace clipdeck {

CommandResult RunShellCommand(std::string command, std::chrono::seconds timeout) {
  const pid_t pid = fork();

  if (pid < 0) {
    return CommandResult{};
  }

  if (pid == 0) {
    execl("/bin/sh", "sh", "-c", command.c_str(), nullptr);
    std::_Exit(127);
  }

  const auto deadline = std::chrono::steady_clock::now() + timeout;
  int status = 0;

  while (std::chrono::steady_clock::now() < deadline) {
    const pid_t result = waitpid(pid, &status, WNOHANG);

    if (result == pid) {
      if (WIFEXITED(status)) {
        return CommandResult{WEXITSTATUS(status), false};
      }

      if (WIFSIGNALED(status)) {
        return CommandResult{128 + WTERMSIG(status), false};
      }

      return CommandResult{-1, false};
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  kill(pid, SIGTERM);
  std::this_thread::sleep_for(std::chrono::milliseconds(250));
  kill(pid, SIGKILL);
  waitpid(pid, &status, 0);

  return CommandResult{-1, true};
}

} // namespace clipdeck
