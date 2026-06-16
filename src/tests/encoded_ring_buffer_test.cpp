#include "../recorder/encoded_ring_buffer.hpp"

#include <chrono>
#include <cstdint>
#include <gtest/gtest.h>
#include <vector>

namespace {

clipdeck::EncodedFragment FragmentAt(
    std::chrono::steady_clock::time_point start, int offset_seconds,
    std::size_t bytes, bool keyframe = false) {
  return clipdeck::EncodedFragment{
      .timestamp = start + std::chrono::seconds(offset_seconds),
      .duration = std::chrono::seconds(1),
      .bytes = std::vector<std::uint8_t>(bytes, 0x42),
      .keyframe = keyframe};
}

} // namespace

TEST(EncodedRingBufferTest, EvictsByTargetDuration) {
  const auto start = std::chrono::steady_clock::now();
  clipdeck::EncodedRingBuffer buffer(std::chrono::seconds(3), 1024);

  buffer.Push(FragmentAt(start, 0, 10));
  buffer.Push(FragmentAt(start, 1, 10));
  buffer.Push(FragmentAt(start, 2, 10));
  buffer.Push(FragmentAt(start, 3, 10));

  EXPECT_EQ(buffer.FragmentCount(), 3);
  EXPECT_EQ(buffer.BufferedDuration(), std::chrono::seconds(3));
}

TEST(EncodedRingBufferTest, EvictsByMemoryBudget) {
  const auto start = std::chrono::steady_clock::now();
  clipdeck::EncodedRingBuffer buffer(std::chrono::seconds(10), 25);

  buffer.Push(FragmentAt(start, 0, 10));
  buffer.Push(FragmentAt(start, 1, 10));
  buffer.Push(FragmentAt(start, 2, 10));

  EXPECT_EQ(buffer.FragmentCount(), 2);
  EXPECT_LE(buffer.ByteSize(), 25);
}

TEST(EncodedRingBufferTest, SelectsLastDurationStartingAtKeyframeWhenAvailable) {
  const auto start = std::chrono::steady_clock::now();
  clipdeck::EncodedRingBuffer buffer(std::chrono::seconds(10), 1024);

  buffer.Push(FragmentAt(start, 0, 10, true));
  buffer.Push(FragmentAt(start, 1, 10));
  buffer.Push(FragmentAt(start, 2, 10, true));
  buffer.Push(FragmentAt(start, 3, 10));
  buffer.Push(FragmentAt(start, 4, 10));

  const auto selected = buffer.SelectLast(std::chrono::seconds(3));

  ASSERT_EQ(selected.size(), 3);
  EXPECT_EQ(selected.front().timestamp, start + std::chrono::seconds(2));
  EXPECT_TRUE(selected.front().keyframe);
}
