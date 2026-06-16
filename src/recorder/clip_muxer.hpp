#pragma once

#include "encoded_ring_buffer.hpp"

#include <filesystem>
#include <optional>
#include <vector>

namespace clipdeck {

class ClipMuxer {
public:
  explicit ClipMuxer(std::filesystem::path clip_directory);

  [[nodiscard]] std::optional<std::filesystem::path>
  WriteClip(const std::vector<EncodedFragment> &fragments) const;

private:
  [[nodiscard]] std::filesystem::path BuildClipPath() const;

  std::filesystem::path clip_directory_;
};

} // namespace clipdeck
