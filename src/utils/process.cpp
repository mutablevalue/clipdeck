#include "process.hpp"

#include "logger.hpp"

#include <algorithm>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <cctype>
#include <ranges>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>

namespace {

constexpr std::string_view kProcessContext = "process";

} // namespace

namespace clipdeck {

std::optional<pid_t> ReadPidFile(const std::filesystem::path &path) {
  std::ifstream input(path);

  if (!input.is_open()) {
    return std::nullopt;
  }

  pid_t pid = 0;
  input >> pid;

  if (!input.good() && !input.eof()) {
    return std::nullopt;
  }

  return pid > 0 ? std::optional<pid_t>{pid} : std::nullopt;
}

bool WritePidFile(const std::filesystem::path &path, pid_t pid) {
  std::error_code error;
  std::filesystem::create_directories(path.parent_path(), error);

  if (error) {
    Log(LogLevel::Error, kProcessContext,
        "Failed to create runtime directory: " + error.message());
    return false;
  }

  std::ofstream output(path, std::ios::trunc);

  if (!output.is_open()) {
    Log(LogLevel::Error, kProcessContext, "Failed to write pid file.");
    return false;
  }

  output << pid << '\n';
  return output.good();
}

void RemovePidFile(const std::filesystem::path &path) {
  std::error_code error;
  std::filesystem::remove(path, error);
}

bool IsProcessRunning(pid_t pid) {
  if (pid <= 0) {
    return false;
  }

  return kill(pid, 0) == 0;
}

bool RequestProcessStop(pid_t pid) {
  if (!IsProcessRunning(pid)) {
    return false;
  }

  return kill(pid, SIGTERM) == 0;
}

bool RedirectStandardStreams(const std::filesystem::path &log_path) {
  std::error_code error;
  std::filesystem::create_directories(log_path.parent_path(), error);

  if (error) {
    return false;
  }

  const int log_descriptor =
      open(log_path.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);

  if (log_descriptor < 0) {
    return false;
  }

  dup2(log_descriptor, STDOUT_FILENO);
  dup2(log_descriptor, STDERR_FILENO);
  close(log_descriptor);
  return true;
}

bool IsProcessNameRunning(const std::string &process_name) {
  std::error_code error;

  for (const auto &entry : std::filesystem::directory_iterator("/proc", error)) {
    if (error || !entry.is_directory()) {
      continue;
    }

    const std::string pid_text = entry.path().filename().string();
    if (pid_text.empty() ||
        !std::ranges::all_of(pid_text, [](unsigned char character) {
          return std::isdigit(character);
        })) {
      continue;
    }

    std::ifstream command_name(entry.path() / "comm");
    std::string name;
    std::getline(command_name, name);

    if (name == process_name) {
      return true;
    }
  }

  return false;
}

std::optional<pid_t>
SpawnDetachedProcess(const std::string &executable,
                     const std::vector<std::string> &arguments,
                     const std::filesystem::path &log_path) {
  const pid_t pid = fork();

  if (pid < 0) {
    return std::nullopt;
  }

  if (pid > 0) {
    return pid;
  }

  setsid();
  RedirectStandardStreams(log_path);

  std::vector<std::string> owned_arguments;
  owned_arguments.reserve(arguments.size() + 1);
  owned_arguments.push_back(executable);
  owned_arguments.insert(owned_arguments.end(), arguments.begin(), arguments.end());

  std::vector<char *> argv;
  argv.reserve(owned_arguments.size() + 1);

  for (auto &argument : owned_arguments) {
    argv.push_back(argument.data());
  }

  argv.push_back(nullptr);
  execvp(executable.c_str(), argv.data());
  std::_Exit(127);
}

} // namespace clipdeck
