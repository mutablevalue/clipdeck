#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace clipdeck {

[[nodiscard]] std::vector<std::string> SplitShellWords(std::string_view text);
[[nodiscard]] std::string JoinCommandPreview(const std::vector<std::string> &parts);

} // namespace clipdeck
