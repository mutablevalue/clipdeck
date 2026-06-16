#pragma once

#include <chrono>
#include <optional>
#include <string>

namespace clipdeck {

[[nodiscard]] std::optional<std::string>
CaptureKeybindFromTerminal(std::chrono::seconds timeout);

} // namespace clipdeck
