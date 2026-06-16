#include "../recorder/recorder_setup.hpp"

#include <gtest/gtest.h>

TEST(RecorderSetupTest, ParsesPulseSourcesAndMarksDefaultMonitor) {
  constexpr std::string_view pactl_sources =
      "0\talsa_output.pci-0000_00_1f.3.analog-stereo.monitor\tPipeWire\n"
      "1\tbluez_output.11_22_33_44_55_66.a2dp-sink.monitor\tPipeWire\n"
      "2\talsa_input.pci-0000_00_1f.3.analog-stereo\tPipeWire\n";

  const auto sources = clipdeck::ParseAudioCaptureSources(
      pactl_sources,
      "bluez_output.11_22_33_44_55_66.a2dp-sink.monitor");

  ASSERT_EQ(sources.size(), 3);
  EXPECT_EQ(sources.at(0).name,
            "alsa_output.pci-0000_00_1f.3.analog-stereo.monitor");
  EXPECT_TRUE(sources.at(0).monitor);
  EXPECT_FALSE(sources.at(0).default_output_monitor);
  EXPECT_EQ(sources.at(1).name,
            "bluez_output.11_22_33_44_55_66.a2dp-sink.monitor");
  EXPECT_TRUE(sources.at(1).monitor);
  EXPECT_TRUE(sources.at(1).default_output_monitor);
  EXPECT_FALSE(sources.at(2).monitor);
}

TEST(RecorderSetupTest, SelectsDefaultOutputMonitorFirst) {
  const std::vector<clipdeck::AudioCaptureSource> sources{
      {.name = "alsa_output.internal.monitor",
       .monitor = true,
       .default_output_monitor = false},
      {.name = "bluez_output.headphones.monitor",
       .monitor = true,
       .default_output_monitor = true},
      {.name = "alsa_input.microphone",
       .monitor = false,
       .default_output_monitor = false},
  };

  EXPECT_EQ(clipdeck::SelectAutomaticAudioMonitor(sources),
            "bluez_output.headphones.monitor");
}

TEST(RecorderSetupTest, FallsBackToFirstMonitorWhenDefaultIsUnavailable) {
  const std::vector<clipdeck::AudioCaptureSource> sources{
      {.name = "alsa_input.microphone",
       .monitor = false,
       .default_output_monitor = false},
      {.name = "alsa_output.internal.monitor",
       .monitor = true,
       .default_output_monitor = false},
  };

  EXPECT_EQ(clipdeck::SelectAutomaticAudioMonitor(sources),
            "alsa_output.internal.monitor");
}

TEST(RecorderSetupTest, RejectsInputOnlySourcesForAutomaticAudio) {
  const std::vector<clipdeck::AudioCaptureSource> sources{
      {.name = "alsa_input.microphone",
       .monitor = false,
       .default_output_monitor = false},
  };

  EXPECT_FALSE(clipdeck::SelectAutomaticAudioMonitor(sources).has_value());
}
