#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <vector>

namespace clipdeck {

struct EncodedFragment {
  std::chrono::steady_clock::time_point timestamp{};
  std::chrono::milliseconds duration{0};
  std::vector<std::uint8_t> bytes;
  bool keyframe = false;
};

class EncodedRingBuffer {
public:
  EncodedRingBuffer(std::chrono::seconds target_duration,
                    std::size_t memory_budget_bytes);

  void Push(EncodedFragment fragment);
  [[nodiscard]] std::vector<EncodedFragment>
  SelectLast(std::chrono::seconds duration) const;
  void Clear();

  [[nodiscard]] std::chrono::milliseconds BufferedDuration() const;
  [[nodiscard]] std::size_t ByteSize() const;
  [[nodiscard]] std::size_t FragmentCount() const;
  [[nodiscard]] std::size_t MemoryBudgetBytes() const;

private:
  void EvictExpired();
  [[nodiscard]] std::chrono::milliseconds BufferedDurationLocked() const;

  std::chrono::seconds target_duration_;
  std::size_t memory_budget_bytes_;
  mutable std::mutex mutex_;
  std::deque<EncodedFragment> fragments_;
  std::size_t byte_size_ = 0;
};

} // namespace clipdeck
