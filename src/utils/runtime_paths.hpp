#pragma once

#include <filesystem>

namespace clipdeck {

[[nodiscard]] std::filesystem::path RuntimeDirectory();
[[nodiscard]] std::filesystem::path PidFilePath();
[[nodiscard]] std::filesystem::path DaemonLogPath();
[[nodiscard]] std::filesystem::path RecorderStatusPath();

} // namespace clipdeck
