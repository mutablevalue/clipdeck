#include "keybind.hpp"

#include <algorithm>
#include <cctype>
#include <linux/input-event-codes.h>
#include <ranges>
#include <string>
#include <unordered_map>

namespace {

std::string Trim(std::string_view value) {
  const auto first =
      std::ranges::find_if(value, [](unsigned char character) {
        return !std::isspace(character);
      });

  const auto last =
      std::ranges::find_if(value | std::views::reverse,
                           [](unsigned char character) {
                             return !std::isspace(character);
                           })
          .base();

  if (first >= last) {
    return {};
  }

  return std::string(first, last);
}

std::string Uppercase(std::string value) {
  std::ranges::transform(value, value.begin(), [](unsigned char character) {
    return static_cast<char>(std::toupper(character));
  });

  return value;
}

std::vector<std::string> SplitTokens(std::string_view keybind) {
  std::vector<std::string> tokens;

  for (const auto token : std::views::split(keybind, '+')) {
    std::string text;
    for (const char character : token) {
      text.push_back(character);
    }

    text = Trim(text);
    if (!text.empty()) {
      tokens.push_back(Uppercase(text));
    }
  }

  return tokens;
}

std::string DisplayToken(std::string token) {
  if (token == "CONTROL") {
    token = "CTRL";
  }

  if (token == "CTRL") {
    return "Ctrl";
  }

  if (token == "ALT") {
    return "Alt";
  }

  if (token == "SHIFT") {
    return "Shift";
  }

  return token;
}

const std::unordered_map<std::string, int> &LetterKeyCodes() {
  static const std::unordered_map<std::string, int> key_codes{
      {"A", KEY_A}, {"B", KEY_B}, {"C", KEY_C}, {"D", KEY_D},
      {"E", KEY_E}, {"F", KEY_F}, {"G", KEY_G}, {"H", KEY_H},
      {"I", KEY_I}, {"J", KEY_J}, {"K", KEY_K}, {"L", KEY_L},
      {"M", KEY_M}, {"N", KEY_N}, {"O", KEY_O}, {"P", KEY_P},
      {"Q", KEY_Q}, {"R", KEY_R}, {"S", KEY_S}, {"T", KEY_T},
      {"U", KEY_U}, {"V", KEY_V}, {"W", KEY_W}, {"X", KEY_X},
      {"Y", KEY_Y}, {"Z", KEY_Z}};

  return key_codes;
}

std::optional<clipdeck::KeyRequirement>
RequirementForToken(const std::string &token) {
  if (token == "CTRL" || token == "CONTROL") {
    return clipdeck::KeyRequirement{"Ctrl", {KEY_LEFTCTRL, KEY_RIGHTCTRL}};
  }

  if (token == "ALT") {
    return clipdeck::KeyRequirement{"Alt", {KEY_LEFTALT, KEY_RIGHTALT}};
  }

  if (token == "SHIFT") {
    return clipdeck::KeyRequirement{"Shift", {KEY_LEFTSHIFT, KEY_RIGHTSHIFT}};
  }

  const auto &key_codes = LetterKeyCodes();
  if (const auto key_code = key_codes.find(token); key_code != key_codes.end()) {
    return clipdeck::KeyRequirement{token, {key_code->second}};
  }

  return std::nullopt;
}

} // namespace

namespace clipdeck {

std::string NormalizeKeybind(std::string_view keybind) {
  std::string normalized;

  for (const auto &token : SplitTokens(keybind)) {
    if (!normalized.empty()) {
      normalized += '+';
    }

    normalized += DisplayToken(token);
  }

  return normalized;
}

std::optional<std::vector<KeyRequirement>>
ParseKeybindRequirements(std::string_view keybind) {
  std::vector<KeyRequirement> requirements;

  for (const auto &token : SplitTokens(keybind)) {
    auto requirement = RequirementForToken(token);

    if (!requirement.has_value()) {
      return std::nullopt;
    }

    requirements.push_back(std::move(requirement.value()));
  }

  if (requirements.empty()) {
    return std::nullopt;
  }

  return requirements;
}

std::optional<std::string> RawCharacterToKeyToken(char character) {
  const auto raw = static_cast<unsigned char>(character);

  if (raw >= 1 && raw <= 26) {
    const char letter = static_cast<char>('A' + raw - 1);
    return std::string("Ctrl+") + letter;
  }

  if (std::isalpha(raw)) {
    return std::string(1, static_cast<char>(std::toupper(raw)));
  }

  return std::nullopt;
}

} // namespace clipdeck
