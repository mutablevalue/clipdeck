# ClipDeck

ClipDeck is a production-ready Linux clipping service written in C++23. It runs as a background daemon, records a rolling buffer of screen and desktop audio, and saves recent clips through a command-line interface or global keybind.

The current release provides the backend clipping service. A graphical interface is planned as the next layer on top of the existing daemon.

## Features

* Background clipping daemon
* Manual clip saves from the CLI
* Global Linux keybind listener
* Screen capture through the desktop portal and PipeWire
* Desktop audio capture through PulseAudio/PipeWire monitor sources
* Rolling segment-based recorder
* Configurable clip length, output path, resolution, FPS, bitrate, encoder, and audio source
* Runtime status and diagnostics

## Requirements

Build requirements:

```text
Linux
CMake
C++23 compiler
pkg-config
GStreamer development headers
FFmpeg / FFprobe
```

Runtime requirements:

```text
PipeWire
WirePlumber or compatible session manager
PulseAudio/PipeWire Pulse compatibility
XDG Desktop Portal
GStreamer runtime plugins
FFmpeg / FFprobe
Read access to /dev/input/event* for global keybinds
```

## Fedora dependencies

```bash
sudo dnf install -y \
  cmake \
  gcc-c++ \
  make \
  pkgconf-pkg-config \
  gstreamer1-devel \
  gstreamer1-plugins-base-devel \
  gstreamer1-plugins-base \
  gstreamer1-plugins-good \
  gstreamer1-plugins-bad-free \
  gstreamer1-plugin-pipewire \
  gstreamer1-plugin-openh264 \
  gstreamer1-libav \
  pipewire \
  pipewire-pulseaudio \
  wireplumber \
  pulseaudio-utils \
  ffmpeg
```

Some Fedora installations may require RPM Fusion for full FFmpeg support.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target clipdeck -j"$(nproc)"
```

For a debug build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target clipdeck -j"$(nproc)"
```

## Basic usage

Run setup:

```bash
./build/clipdeck setup
```

Start the daemon:

```bash
./build/clipdeck start
```

Check status:

```bash
./build/clipdeck status
```

Save a clip manually:

```bash
./build/clipdeck save
```

Stop the daemon:

```bash
./build/clipdeck stop
```

Restart the daemon:

```bash
./build/clipdeck restart
```

Run diagnostics:

```bash
./build/clipdeck diagnose
```

## Commands

```text
clipdeck start                         Start the background listener
clipdeck stop                          Stop the background listener
clipdeck restart                       Restart the background listener
clipdeck save                          Request a clip save
clipdeck status                        Show runtime status and settings
clipdeck setup                         Validate and save native capture sources
clipdeck diagnose                      Check recorder configuration
clipdeck settings                      Show saved settings
clipdeck settings show                 Show saved settings
clipdeck settings keybind              Capture and save the save keybind
clipdeck settings keybind <combo>      Save a keybind directly
clipdeck settings length <seconds>     Set the clip length
clipdeck settings buffer-safety <sec>  Set extra recorder buffer duration
clipdeck settings output <directory>   Set the clip save directory
clipdeck settings video-source portal  Use screen capture through desktop portal
clipdeck settings audio <mode>         Enable, disable, auto-select, or list audio
clipdeck settings audio-source <src>   Set desktop audio monitor source
clipdeck settings resolution <w> <h>   Set capture resolution
clipdeck settings fps <fps>            Set capture frame rate
clipdeck settings video-bitrate <kbps> Set video bitrate
clipdeck settings audio-bitrate <kbps> Set audio bitrate
clipdeck settings encoder <encoder>    Set encoder
clipdeck help                          Show help
```

## Settings

Show current settings:

```bash
./build/clipdeck settings
```

Set clip length:

```bash
./build/clipdeck settings length 30
```

Set output directory:

```bash
./build/clipdeck settings output ~/Videos/ClipDeck
```

Set capture resolution:

```bash
./build/clipdeck settings resolution 1920 1080
```

Set capture FPS:

```bash
./build/clipdeck settings fps 60
```

Set video bitrate:

```bash
./build/clipdeck settings video-bitrate 12000
```

Set audio bitrate:

```bash
./build/clipdeck settings audio-bitrate 192
```

Set encoder:

```bash
./build/clipdeck settings encoder x264
```

```bash
./build/clipdeck settings encoder openh264
```

## Audio

Enable audio:

```bash
./build/clipdeck settings audio on
```

Disable audio:

```bash
./build/clipdeck settings audio off
```

Automatically select a desktop audio monitor:

```bash
./build/clipdeck settings audio auto
```

List available desktop audio monitor sources:

```bash
./build/clipdeck settings audio devices
```

Set a specific audio source:

```bash
./build/clipdeck settings audio-source <monitor-source>
```

## Keybinds

Set a keybind directly:

```bash
./build/clipdeck settings keybind Ctrl+Z+P
```

Capture a keybind from the terminal:

```bash
./build/clipdeck settings keybind
```

Terminal capture only saves the configured keybind. It does not verify that the daemon has permission to read global input events.

Supported keybind tokens:

```text
Ctrl
Alt
Shift
A-Z
```

Examples:

```bash
./build/clipdeck settings keybind Ctrl+Alt+S
./build/clipdeck settings keybind Ctrl+Z+P
./build/clipdeck settings keybind Shift+F
```

## Global input permissions

Runtime global keybinds require read access to Linux input event devices.

Check permissions:

```bash
groups
ls -l /dev/input/event*
```

Add your user to the `input` group if needed:

```bash
sudo usermod -aG input "$USER"
```

Log out and log back in after changing groups.

## Runtime files

ClipDeck stores daemon runtime data under the local user state directory.

Common development path:

```text
~/.local/state/clipdeck/
```

Daemon log:

```text
~/.local/state/clipdeck/daemon.log
```

Watch logs:

```bash
tail -f ~/.local/state/clipdeck/daemon.log
```

## Troubleshooting

### Keybind does not work

Check that the daemon is running:

```bash
./build/clipdeck status
```

Check input permissions:

```bash
groups
ls -l /dev/input/event*
```

Check logs:

```bash
tail -f ~/.local/state/clipdeck/daemon.log
```

Common causes:

```text
The daemon is not running.
The user cannot read /dev/input/event*.
The user was added to input but has not logged out and back in.
The keybind uses unsupported keys.
```

### No audio is recorded

List audio monitor sources:

```bash
./build/clipdeck settings audio devices
```

Enable audio:

```bash
./build/clipdeck settings audio on
```

Use automatic audio source selection:

```bash
./build/clipdeck settings audio auto
```

Run setup again:

```bash
./build/clipdeck setup
```

### Clip output is black or blank

Check status:

```bash
./build/clipdeck status
```

Check logs:

```bash
tail -f ~/.local/state/clipdeck/daemon.log
```

Restart the daemon:

```bash
./build/clipdeck restart
```

Black output usually indicates a capture, portal, PipeWire, compositor, driver, or segment validation issue.

## Development

Recommended checks before pushing:

```bash
cmake --build build --target clipdeck -j"$(nproc)"
cmake --build build --target clipdeck_tests -j"$(nproc)"
ctest --test-dir build --output-on-failure
git diff --check
```

## Roadmap

```text
Graphical interface
Tray controls
Default keybind feedback sound
Installer/package support
Systemd user service
Additional media validation tests
```

## License

No license has been selected yet.

Add a `LICENSE` file before distributing ClipDeck as open source.
