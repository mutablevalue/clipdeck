#include "logger.hpp"

namespace clipdeck {

Logger &Logger::Instance() {
  static Logger logger;
  return logger;
}

void Logger::SetLogFile(const std::filesystem::path &path) {
  std::scoped_lock lock(mutex_);

  std::error_code error;
  std::filesystem::create_directories(path.parent_path(), error);
  if (error) {
    return;
  }

  file_stream_.close();
  file_stream_.open(path, std::ios::app);
}

void Logger::ClearLogFile() {
  std::scoped_lock lock(mutex_);
  file_stream_.close();
}

void Logger::Write(LogLevel level, std::string_view context,
                   std::string_view message) {
  std::scoped_lock lock(mutex_);
  std::ostream &output = OutputStream(level);

  output << "[" << GetCurrentTime() << "] "
         << "[" << GetLogLevelName(level) << "] "
         << "[" << context << "] " << message << '\n'
         << std::flush;
}

void Logger::Debug(std::string_view context, std::string_view message) {
  Write(LogLevel::Debug, context, message);
}

void Logger::Info(std::string_view context, std::string_view message) {
  Write(LogLevel::Info, context, message);
}

void Logger::Warning(std::string_view context, std::string_view message) {
  Write(LogLevel::Warning, context, message);
}

void Logger::Error(std::string_view context, std::string_view message) {
  Write(LogLevel::Error, context, message);
}

std::ostream &Logger::OutputStream(LogLevel level) {
  if (file_stream_.is_open()) {
    return file_stream_;
  }

  if (level == LogLevel::Error) {
    return std::cerr;
  }

  return std::cout;
}

std::string_view Logger::GetLogLevelName(LogLevel level) {
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

std::string Logger::GetCurrentTime() {
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

} // namespace clipdeck
