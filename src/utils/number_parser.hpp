#pragma once

#include <string_view>

namespace clipdeck {

bool ParsePositiveInteger(std::string_view value, int &number);

} // namespace clipdeck
