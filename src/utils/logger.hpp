#pragma once

#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>

namespace clipdeck {

enum class LogLevel { Debug, Info, Warning, Error };

class Logger {
public:
  static Logger &Instance();

  Logger(const Logger &) = delete;
  Logger &operator=(const Logger &) = delete;

  void SetLogFile(const std::filesystem::path &path);
  void ClearLogFile();

  void Write(LogLevel level, std::string_view context, std::string_view message);
  void Debug(std::string_view context, std::string_view message);
  void Info(std::string_view context, std::string_view message);
  void Warning(std::string_view context, std::string_view message);
  void Error(std::string_view context, std::string_view message);

private:
  Logger() = default;

  [[nodiscard]] std::ostream &OutputStream(LogLevel level);
  [[nodiscard]] static std::string_view GetLogLevelName(LogLevel level);
  [[nodiscard]] static std::string GetCurrentTime();

  std::mutex mutex_;
  std::ofstream file_stream_;
};

inline void Log(LogLevel level, std::string_view context,
                std::string_view message) {
  Logger::Instance().Write(level, context, message);
}

} // namespace clipdeck

using clipdeck::Log;
using clipdeck::LogLevel;
using clipdeck::Logger;
