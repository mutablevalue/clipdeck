#pragma once

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

enum class LogLevel { Debug, Info, Warning, Error };

class Log {
public:
  Log(LogLevel level, std::string_view context, std::string_view message) {
    std::ostream &output_stream = GetOutputStream(level);

    output_stream << "[" << GetCurrentTime() << "] "
                  << "[" << GetLogLevelName(level) << "] "
                  << "[" << context << "] " << message << '\n'
                  << std::flush;
  }

private:
  static std::ostream &GetOutputStream(LogLevel level) {
    if (level == LogLevel::Error) {
      return std::cerr;
    }

    return std::cout;
  }

  static std::string_view GetLogLevelName(LogLevel level) {
    switch (level) {
    case LogLevel::Debug:
      return "DEBUG";
    case LogLevel::Info:
      return "INFO";
    case LogLevel::Warning:
      return "WARN";
    case LogLevel::Error:
      return "ERROR";
    }

    return "UNKNOWN";
  }

  static std::string GetCurrentTime() {
    const auto current_time_point = std::chrono::system_clock::now();
    const auto current_time =
        std::chrono::system_clock::to_time_t(current_time_point);

    std::tm local_time{};

#if defined(_WIN32)
    localtime_s(&local_time, &current_time);
#else
    localtime_r(&current_time, &local_time);
#endif

    std::ostringstream time_stream;
    time_stream << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S");

    return time_stream.str();
  }
};
