#include "gstreamer_recorder.hpp"

#include "../utils/logger.hpp"

#include <format>
#include <ranges>
#include <sstream>
#include <utility>

#if defined(CLIPDECK_HAS_GSTREAMER)
#include <gst/app/gstappsink.h>
#include <gst/gst.h>
#endif

namespace {

constexpr std::string_view kRecorderContext = "native-recorder";

std::chrono::seconds TargetBufferDuration(
    const clipdeck::RecorderConfig &config) {
  return std::chrono::seconds(config.clip_length_seconds +
                              config.buffer_safety_seconds);
}

std::string GstQuote(const std::string &value) {
  std::string quoted = "\"";
  for (const char character : value) {
    if (character == '"' || character == '\\') {
      quoted.push_back('\\');
    }
    quoted.push_back(character);
  }
  quoted.push_back('"');
  return quoted;
}

#if defined(CLIPDECK_HAS_GSTREAMER)
bool ContainsBoxType(const std::vector<std::uint8_t> &bytes,
                     std::string_view box_type) {
  return std::ranges::search(bytes, box_type,
                             [](std::uint8_t left, char right) {
                               return left == static_cast<std::uint8_t>(right);
                             })
             .begin() != bytes.end();
}

bool LooksLikeMp4Initialization(const std::vector<std::uint8_t> &bytes) {
  return (ContainsBoxType(bytes, "ftyp") || ContainsBoxType(bytes, "moov")) &&
         !ContainsBoxType(bytes, "mdat");
}

GstFlowReturn OnNewSample(GstAppSink *sink, gpointer user_data) {
  auto *recorder = static_cast<clipdeck::GStreamerRecorder *>(user_data);
  GstSample *sample = gst_app_sink_pull_sample(sink);

  if (sample == nullptr) {
    return GST_FLOW_ERROR;
  }

  recorder->HandleSample(sample);
  gst_sample_unref(sample);
  return GST_FLOW_OK;
}

void EnsureGStreamerInitialized() {
  static std::once_flag init_flag;
  std::call_once(init_flag, [] {
    GError *error = nullptr;
    if (!gst_init_check(nullptr, nullptr, &error)) {
      const std::string message =
          error == nullptr ? "unknown error" : error->message;
      if (error != nullptr) {
        g_error_free(error);
      }

      Log(LogLevel::Error, kRecorderContext,
          "Failed to initialize GStreamer: " + message);
    }
  });
}
#endif

} // namespace

namespace clipdeck {

GStreamerRecorder::GStreamerRecorder(RecorderConfig config)
    : config_(std::move(config)),
      ring_buffer_(TargetBufferDuration(config_),
                   EstimateRecorderMemoryBudgetBytes(config_)),
      muxer_(config_.clip_directory) {}

GStreamerRecorder::~GStreamerRecorder() { Stop(); }

bool GStreamerRecorder::Start() {
  if (running_) {
    Log(LogLevel::Warning, kRecorderContext,
        "Native recorder is already running.");
    return true;
  }

#if !defined(CLIPDECK_HAS_GSTREAMER)
  SetMessage("GStreamer support was not compiled into this build.", false);
  Log(LogLevel::Error, kRecorderContext, message_);
  return false;
#else
  EnsureGStreamerInitialized();

  GError *error = nullptr;
  const std::string pipeline_description = BuildPipelineDescription();
  GstElement *pipeline = gst_parse_launch(pipeline_description.c_str(), &error);

  if (pipeline == nullptr) {
    const std::string message =
        error == nullptr ? "unknown parse error" : error->message;
    if (error != nullptr) {
      g_error_free(error);
    }

    SetMessage("Failed to build GStreamer pipeline: " + message, false);
    Log(LogLevel::Error, kRecorderContext, message_);
    return false;
  }

  GstElement *appsink =
      gst_bin_get_by_name(GST_BIN(pipeline), "clipdeck_sink");
  if (appsink == nullptr) {
    gst_object_unref(pipeline);
    SetMessage("GStreamer pipeline does not expose the appsink.", false);
    Log(LogLevel::Error, kRecorderContext, message_);
    return false;
  }

  GstAppSinkCallbacks callbacks{};
  callbacks.new_sample = OnNewSample;
  gst_app_sink_set_callbacks(GST_APP_SINK(appsink), &callbacks, this, nullptr);

  GstBus *bus = gst_element_get_bus(pipeline);
  const GstStateChangeReturn state_result =
      gst_element_set_state(pipeline, GST_STATE_PLAYING);

  if (state_result == GST_STATE_CHANGE_FAILURE) {
    gst_object_unref(bus);
    gst_object_unref(appsink);
    gst_object_unref(pipeline);
    SetMessage("GStreamer pipeline failed to enter PLAYING state.", false);
    Log(LogLevel::Error, kRecorderContext, message_);
    return false;
  }

  pipeline_ = pipeline;
  appsink_ = appsink;
  bus_ = bus;
  running_ = true;
  SetMessage("recording", true);
  bus_thread_ =
      std::jthread([this](std::stop_token stop_token) { MonitorBus(stop_token); });

  Log(LogLevel::Info, kRecorderContext,
      std::format("Started native recorder at {}x{}@{}fps, {} kbps video.",
                  config_.width, config_.height, config_.fps,
                  config_.video_bitrate_kbps));
  return true;
#endif
}

void GStreamerRecorder::Stop() {
  if (!running_ && pipeline_ == nullptr) {
    return;
  }

  running_ = false;

  if (bus_thread_.joinable()) {
    bus_thread_.request_stop();
    bus_thread_.join();
  }

#if defined(CLIPDECK_HAS_GSTREAMER)
  if (pipeline_ != nullptr) {
    auto *pipeline = static_cast<GstElement *>(pipeline_);
    gst_element_set_state(pipeline, GST_STATE_NULL);
  }

  if (bus_ != nullptr) {
    gst_object_unref(static_cast<GstBus *>(bus_));
  }

  if (appsink_ != nullptr) {
    gst_object_unref(static_cast<GstElement *>(appsink_));
  }

  if (pipeline_ != nullptr) {
    gst_object_unref(static_cast<GstElement *>(pipeline_));
  }
#endif

  bus_ = nullptr;
  appsink_ = nullptr;
  pipeline_ = nullptr;
  SetMessage("stopped", false);
  Log(LogLevel::Info, kRecorderContext, "Stopped native recorder.");
}

bool GStreamerRecorder::SaveClip() {
  auto fragments =
      ring_buffer_.SelectLast(std::chrono::seconds(config_.clip_length_seconds));

  {
    std::scoped_lock lock(initialization_mutex_);
    fragments.insert(fragments.begin(), initialization_fragments_.begin(),
                     initialization_fragments_.end());
  }

  const auto clip_path = muxer_.WriteClip(fragments);

  if (!clip_path.has_value()) {
    Log(LogLevel::Error, kRecorderContext, "Native clip save failed.");
    return false;
  }

  Log(LogLevel::Info, kRecorderContext,
      "Stored clip: " + clip_path.value().string() + ".");
  return true;
}

RecorderStatus GStreamerRecorder::Status() const {
  std::scoped_lock lock(state_mutex_);
  return RecorderStatus{.running = running_,
                        .healthy = healthy_,
                        .backend = "native",
                        .message = message_,
                        .buffered_duration = ring_buffer_.BufferedDuration(),
                        .buffered_bytes = ring_buffer_.ByteSize(),
                        .memory_budget_bytes =
                            ring_buffer_.MemoryBudgetBytes()};
}

void GStreamerRecorder::MonitorBus(std::stop_token stop_token) {
#if defined(CLIPDECK_HAS_GSTREAMER)
  auto *bus = static_cast<GstBus *>(bus_);

  while (running_ && !stop_token.stop_requested()) {
    GstMessage *message =
        gst_bus_timed_pop_filtered(bus, 250 * GST_MSECOND,
                                   static_cast<GstMessageType>(GST_MESSAGE_ERROR |
                                                               GST_MESSAGE_EOS));

    if (message == nullptr) {
      continue;
    }

    if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_ERROR) {
      GError *error = nullptr;
      gchar *debug = nullptr;
      gst_message_parse_error(message, &error, &debug);
      const std::string text =
          error == nullptr ? "unknown GStreamer error" : error->message;
      SetMessage(text, false);
      Log(LogLevel::Error, kRecorderContext,
          "GStreamer recorder error: " + text);
      if (debug != nullptr) {
        g_free(debug);
      }
      if (error != nullptr) {
        g_error_free(error);
      }
      running_ = false;
    }

    if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_EOS) {
      SetMessage("GStreamer pipeline reached end of stream.", false);
      running_ = false;
    }

    gst_message_unref(message);
  }
#else
  (void)stop_token;
#endif
}

void GStreamerRecorder::HandleSample(void *sample) {
#if defined(CLIPDECK_HAS_GSTREAMER)
  auto *gst_sample = static_cast<GstSample *>(sample);
  GstBuffer *buffer = gst_sample_get_buffer(gst_sample);

  if (buffer == nullptr) {
    return;
  }

  GstMapInfo map_info{};
  if (!gst_buffer_map(buffer, &map_info, GST_MAP_READ)) {
    return;
  }

  EncodedFragment fragment;
  fragment.timestamp = std::chrono::steady_clock::now();
  fragment.duration = GST_CLOCK_TIME_IS_VALID(GST_BUFFER_DURATION(buffer))
                          ? std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::nanoseconds(
                                    GST_BUFFER_DURATION(buffer)))
                          : std::chrono::milliseconds(1000 / config_.fps);
  fragment.keyframe = !GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_DELTA_UNIT);
  fragment.bytes.assign(map_info.data, map_info.data + map_info.size);
  gst_buffer_unmap(buffer, &map_info);

  if (!fragment.bytes.empty()) {
    if (LooksLikeMp4Initialization(fragment.bytes)) {
      std::scoped_lock lock(initialization_mutex_);
      initialization_fragments_.clear();
      initialization_fragments_.push_back(std::move(fragment));
      return;
    }

    ring_buffer_.Push(std::move(fragment));
  }
#else
  (void)sample;
#endif
}

std::string GStreamerRecorder::BuildPipelineDescription() const {
  const std::string video_source =
      config_.video_source.empty() || config_.video_source == "portal"
          ? "pipewiresrc do-timestamp=true"
          : "pipewiresrc path=" + GstQuote(config_.video_source) +
                " do-timestamp=true";

  const std::string video_encoder =
      config_.encoder == "x264"
          ? std::format("x264enc tune=zerolatency speed-preset=veryfast bitrate={} "
                        "key-int-max={} ! h264parse config-interval=-1",
                        config_.video_bitrate_kbps, config_.fps * 2)
          : std::format("openh264enc bitrate={} gop-size={} ! h264parse "
                        "config-interval=-1",
                        config_.video_bitrate_kbps * 1000, config_.fps * 2);

  std::ostringstream pipeline;
  pipeline << "mp4mux name=mux fragment-duration=1000 streamable=true "
           << "! appsink name=clipdeck_sink emit-signals=true sync=false "
           << "max-buffers=" << (config_.fps * 4) << " drop=false "
           << video_source
           << " ! videoconvert ! videoscale ! video/x-raw,width="
           << config_.width << ",height=" << config_.height
           << ",framerate=" << config_.fps << "/1 "
           << "! queue ! " << video_encoder << " ! queue ! mux.video_0 ";

  if (!config_.audio_source.empty()) {
    pipeline << "pulsesrc device=" << GstQuote(config_.audio_source)
             << " do-timestamp=true "
             << "! audioconvert ! audioresample ! avenc_aac bitrate="
             << (config_.audio_bitrate_kbps * 1000)
             << " ! aacparse ! queue ! mux.audio_0 ";
  }

  return pipeline.str();
}

void GStreamerRecorder::SetMessage(std::string message, bool healthy) {
  std::scoped_lock lock(state_mutex_);
  message_ = std::move(message);
  healthy_ = healthy;
}

} // namespace clipdeck
