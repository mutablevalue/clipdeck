#include "number_parser.hpp"

#include <charconv>

namespace clipdeck {

bool ParsePositiveInteger(std::string_view value, int &number) {
  const char *begin = value.data();
  const char *end = begin + value.size();

  const auto parse_result = std::from_chars(begin, end, number);
  return parse_result.ec == std::errc{} && parse_result.ptr == end &&
         number > 0;
}

} // namespace clipdeck
