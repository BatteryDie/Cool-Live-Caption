## Cool Live Caption

A free and open source live caption desktop application that converts audio from your microphone or system audio to text in real-time. The speech recognition is powered by april-asr library with ONNX format. All processed on-device using your CPU.

### OS Audio API
- Windows: WASAPI (loopback + microphone)
- Linux: PulseAudio (loopback + microphone), PipeWire (loopback + microphone)
- macOS: Need research

### Location for Models, Transcripts, and Settings
- Models are loaded from your per-user models folder:
  - Windows: `%LOCALAPPDATA%/coollivecaption/models`
  - macOS: `~/Library/Application Support/com.batterydie.coollivecaption/models`
  - Linux: `~/.coollivecaption/models`
- Transcripts are saved to `{Documents}/Cool Live Caption/transcription-{timestamp}.txt` by default.
- Settings are stored in the user config directory:
  - Windows: `%LOCALAPPDATA%/coollivecaption/settings.ini`
  - macOS: `~/Library/Application Support/com.batterydie.coollivecaption/settings.ini`
  - Linux: `~/.config/coollivecaption/settings.ini`

### Requirements
- CMake 3.24
- C++20 toolchain (GCC/Clang/MinGW)
- ONNX Runtime (auto-fetched by cmake if not provided)
- april-asr (auto-fetched by cmake if not provided)

## Quick Start

1. Download the latest release for your platform.
2. Install and launch the app.
3. Download `april-english-dev-01110_en.april` model from link: https://abb128.github.io/april-asr/models.html
4. Place the model file into your models folder (created on first run or by installer)
   - Windows: `%LOCALAPPDATA%/coollivecaption/models`
   - macOS: `~/Library/Application Support/com.batterydie.coollivecaption/models`
   - Linux: `~/.coollivecaption/models`

### Quick Start for Developers
1. Clone the repo.
2. Configure and build with your preferred GCC/Clang toolchain (examples below). Run the produced `bin/coollivecaption` executable.
3. Add at least one model file into your models folder (created on first run or by installer)
   - Windows: `%LOCALAPPDATA%/coollivecaption/models`
   - macOS: `~/Library/Application Support/com.batterydie.coollivecaption/models`
   - Linux: `~/.coollivecaption/models`

### Build: Windows (MinGW)

```powershell
cmake -S . -B build-windows-release -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build-windows-release
```
```powershell
cmake -S . -B build-windows-debug -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug
cmake --build build-windows-debug
```

The build copies ONNX/april-asr DLLs and MinGW runtimes (`libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll`, `libsrdc.dll`) next to the exe for redistribution.

### Build: Linux (GCC/Clang)

```bash
cmake -S . -B build-linux-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-linux-release
```
```bash
cmake -S . -B build-linux-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-linux-debug
```

To build and test AppImage, please refer to `deploy/build_appimage.sh` script.

### Build: macOS (Clang)

```bash
cmake -S . -B build-macos-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-macos-release
```
```bash
cmake -S . -B build-macos-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-macos-debug
```

### Run
Place models in your per-user models folder (see above). Launch the app; choose Audio Source and Caption Model from the menubar. Captions appear in the main window and are saved to `{Documents}/Cool Live Caption/transcription-{timestamp}.md`.

## Acknowledgements

This project makes use of a few libraries:

- [april-asr](https://github.com/abb128/april-asr) - for on-device speech-to-text/speech recognition (License: GPL-3.0, © abb128 and contributors)
- [ONNX Runtime](https://onnxruntime.ai/) - for running ONNX models efficiently (License: MIT, © Microsoft Corporation)
- [Dear ImGui](https://github.com/ocornut/imgui) - for the GUI framework (License: MIT, © Omar Cornut and contributors)

I would like to thank to abb128 (and contributors of april-asr) for creating april-asr libary.

## License

Cool Live Caption is free software licensed under GPL-3.0. See [LICENSE](LICENSE) for details.