#include "string_utils.hpp"

#include <cctype>

namespace clipdeck {

std::vector<std::string> SplitShellWords(std::string_view text) {
  std::vector<std::string> words;
  std::string current;
  char quote = '\0';
  bool escaping = false;

  for (const char character : text) {
    if (escaping) {
      current.push_back(character);
      escaping = false;
      continue;
    }

    if (character == '\\') {
      escaping = true;
      continue;
    }

    if (quote != '\0') {
      if (character == quote) {
        quote = '\0';
      } else {
        current.push_back(character);
      }
      continue;
    }

    if (character == '\'' || character == '"') {
      quote = character;
      continue;
    }

    if (std::isspace(static_cast<unsigned char>(character))) {
      if (!current.empty()) {
        words.push_back(current);
        current.clear();
      }
      continue;
    }

    current.push_back(character);
  }

  if (!current.empty()) {
    words.push_back(current);
  }

  return words;
}

std::string JoinCommandPreview(const std::vector<std::string> &parts) {
  std::string preview;

  for (const auto &part : parts) {
    if (!preview.empty()) {
      preview += ' ';
    }

    preview += part;
  }

  return preview;
}

} // namespace clipdeck
