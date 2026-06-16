#include "runtime_paths.hpp"

namespace clipdeck {

std::filesystem::path RuntimeDirectory() { return "output/runtime"; }

std::filesystem::path PidFilePath() { return RuntimeDirectory() / "clipdeck.pid"; }

std::filesystem::path DaemonLogPath() {
  return RuntimeDirectory() / "clipdeck-daemon.log";
}

std::filesystem::path RecorderStatusPath() {
  return RuntimeDirectory() / "recorder-status.conf";
}

} // namespace clipdeck
