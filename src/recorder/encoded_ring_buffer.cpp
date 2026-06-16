#include "encoded_ring_buffer.hpp"

#include <algorithm>

namespace clipdeck {

EncodedRingBuffer::EncodedRingBuffer(std::chrono::seconds target_duration,
                                     std::size_t memory_budget_bytes)
    : target_duration_(target_duration),
      memory_budget_bytes_(memory_budget_bytes) {}

void EncodedRingBuffer::Push(EncodedFragment fragment) {
  std::scoped_lock lock(mutex_);
  byte_size_ += fragment.bytes.size();
  fragments_.push_back(std::move(fragment));
  EvictExpired();
}

std::vector<EncodedFragment>
EncodedRingBuffer::SelectLast(std::chrono::seconds duration) const {
  std::scoped_lock lock(mutex_);

  if (fragments_.empty()) {
    return {};
  }

  const auto end_time = fragments_.back().timestamp + fragments_.back().duration;
  const auto start_time = end_time - duration;

  auto first = std::ranges::find_if(fragments_, [start_time](const auto &fragment) {
    return fragment.timestamp + fragment.duration >= start_time;
  });

  if (first == fragments_.end()) {
    return {};
  }

  auto keyframe = first;
  for (auto current = first; current != fragments_.end(); ++current) {
    if (current->keyframe) {
      keyframe = current;
      break;
    }
  }

  return {keyframe, fragments_.end()};
}

void EncodedRingBuffer::Clear() {
  std::scoped_lock lock(mutex_);
  fragments_.clear();
  byte_size_ = 0;
}

std::chrono::milliseconds EncodedRingBuffer::BufferedDuration() const {
  std::scoped_lock lock(mutex_);
  return BufferedDurationLocked();
}

std::size_t EncodedRingBuffer::ByteSize() const {
  std::scoped_lock lock(mutex_);
  return byte_size_;
}

std::size_t EncodedRingBuffer::FragmentCount() const {
  std::scoped_lock lock(mutex_);
  return fragments_.size();
}

std::size_t EncodedRingBuffer::MemoryBudgetBytes() const {
  return memory_budget_bytes_;
}

void EncodedRingBuffer::EvictExpired() {
  while (!fragments_.empty() &&
         (BufferedDurationLocked() > target_duration_ ||
          byte_size_ > memory_budget_bytes_)) {
    byte_size_ -= fragments_.front().bytes.size();
    fragments_.pop_front();
  }
}

std::chrono::milliseconds EncodedRingBuffer::BufferedDurationLocked() const {
  if (fragments_.empty()) {
    return std::chrono::milliseconds(0);
  }

  const auto start_time = fragments_.front().timestamp;
  const auto end_time = fragments_.back().timestamp + fragments_.back().duration;
  return std::chrono::duration_cast<std::chrono::milliseconds>(end_time -
                                                               start_time);
}

} // namespace clipdeck
