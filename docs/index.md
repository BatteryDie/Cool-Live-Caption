---
layout: default
title: Cool Live Captions
---

# Welcome

<p style="text-align:center;">
   <img src="assets/icon.png" alt="Cool Live Captions icon" />
</p>

> Note: Cool Live Captions is experimental and captions may not be 100% accurate.

A free and open source live caption desktop application that converts audio from your microphone or system audio to text in real-time. The speech recognition is powered by april-asr library with ONNX format. All processed on-device using your CPU.

Windows and Linux are currently supported. macOS is coming soon.

## Feature
- Dear ImGui GUI framework
- Desktop Audio and Microphone Capture Support
- Autosave transcripts in text file
- Profanity filter

## Coming Soon
- More language models
- Model Manager + Automatic update
- Capture from specific applications (depending on OS capabilities)
- macOS support
- Application Updater (Windows and macOS only)
- Flathub support (Linux only)

## Screenshot

| ![Screenshot of Cool Live Captions on Windows](assets/screenshot01.png) | ![Screenshot of Cool Live Captions on Linux GNOME](assets/screenshot02.png) |
| --- | --- |
| Windows 11 | Linux GNOME |

## Quick Start

1. Download the latest release for your platform.
2. Install and launch the app.
3. Download `april-english-dev-01110_en.april` model from link: https://abb128.github.io/april-asr/models.html
4. Place the model file into your models folder (created on first run or by installer):
   - Windows: `%LOCALAPPDATA%/coollivecaptions/models`
   - macOS: `~/Library/Application Support/com.batterydie.coollivecaptions/models`
   - Linux: `~/.coollivecaptions/models`

Any issue, please sumbit on the [GitHub Issues page](https://github.com/batterydie/cool-live-captions/issues).

## Download

Grab the latest build on the [Releases page](https://github.com/batterydie/cool-live-captions/releases).

> `.EXE` is an installer for Windows, `.AppImage` is a portable application for Linux, and `.DMG` is an image file for macOS.

For AppImage file, set executable permission via GUI or commandline before running.

## Acknowledgements

This project makes use of a few libraries:

- [april-asr](https://github.com/abb128/april-asr) - for on-device speech-to-text/speech recognition (License: GPL-3.0, © abb128 and contributors)
- [ONNX Runtime](https://onnxruntime.ai/) - for running ONNX models efficiently (License: MIT, © Microsoft Corporation)
- [Dear ImGui](https://github.com/ocornut/imgui) - for the GUI framework (License: MIT, © Omar Cornut and contributors)

I would like to thank to abb128 (and contributors of april-asr) for creating april-asr libary.

## License

Cool Live Captions is free software licensed under GPL-3.0. See [LICENSE](LICENSE) for details.
