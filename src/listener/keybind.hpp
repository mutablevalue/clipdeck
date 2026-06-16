#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace clipdeck {

struct KeyRequirement {
  std::string label;
  std::vector<int> alternatives;
};

[[nodiscard]] std::string NormalizeKeybind(std::string_view keybind);
[[nodiscard]] std::optional<std::vector<KeyRequirement>>
ParseKeybindRequirements(std::string_view keybind);
[[nodiscard]] std::optional<std::string> RawCharacterToKeyToken(char character);

} // namespace clipdeck
