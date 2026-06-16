#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include <sys/types.h>

namespace clipdeck {

[[nodiscard]] std::optional<pid_t> ReadPidFile(const std::filesystem::path &path);
bool WritePidFile(const std::filesystem::path &path, pid_t pid);
void RemovePidFile(const std::filesystem::path &path);
[[nodiscard]] bool IsProcessRunning(pid_t pid);
bool RequestProcessStop(pid_t pid);
bool RedirectStandardStreams(const std::filesystem::path &log_path);
[[nodiscard]] bool IsProcessNameRunning(const std::string &process_name);
[[nodiscard]] std::optional<pid_t>
SpawnDetachedProcess(const std::string &executable,
                     const std::vector<std::string> &arguments,
                     const std::filesystem::path &log_path);

} // namespace clipdeck
