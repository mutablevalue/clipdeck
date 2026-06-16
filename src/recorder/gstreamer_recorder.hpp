#pragma once

#include "clip_muxer.hpp"
#include "encoded_ring_buffer.hpp"
#include "recorder_backend.hpp"

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace clipdeck {

class GStreamerRecorder final : public RecorderBackend {
public:
  explicit GStreamerRecorder(RecorderConfig config);
  ~GStreamerRecorder() override;

  GStreamerRecorder(const GStreamerRecorder &) = delete;
  GStreamerRecorder &operator=(const GStreamerRecorder &) = delete;

  bool Start() override;
  void Stop() override;
  bool SaveClip() override;
  [[nodiscard]] RecorderStatus Status() const override;
  void HandleSample(void *sample);

private:
  void MonitorBus(std::stop_token stop_token);
  [[nodiscard]] std::string BuildPipelineDescription() const;
  void SetMessage(std::string message, bool healthy);

  RecorderConfig config_;
  EncodedRingBuffer ring_buffer_;
  ClipMuxer muxer_;
  mutable std::mutex state_mutex_;
  mutable std::mutex initialization_mutex_;
  std::vector<EncodedFragment> initialization_fragments_;
  std::atomic_bool running_{false};
  bool healthy_ = false;
  std::string message_ = "not started";
  void *pipeline_ = nullptr;
  void *appsink_ = nullptr;
  void *bus_ = nullptr;
  std::jthread bus_thread_;
};

} // namespace clipdeck
