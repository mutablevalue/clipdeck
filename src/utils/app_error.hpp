#pragma once

#include "logger.hpp"

#include <string>
#include <string_view>

namespace clipdeck {

struct AppError {
  std::string code;
  std::string context;
  std::string message;
};

inline AppError MakeError(std::string_view code, std::string_view context,
                          std::string_view message) {
  return AppError{std::string(code), std::string(context),
                  std::string(message)};
}

inline void HandleError(const AppError &error) {
  Log(LogLevel::Error, error.context,
      error.code.empty() ? error.message : error.code + ": " + error.message);
}

} // namespace clipdeck
