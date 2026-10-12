# VidCut

Fast video cutter (Qt 6 Widgets, C++17) for Windows, Linux and macOS.
The keyframe-aligned middle of the clip is copied without re-encoding; only the
short edges are re-encoded. Drop a video on the window (or use Browse), adjust
Start/End, press Cut.

Needs **FFmpeg** (`ffmpeg` and `ffprobe`, FFmpeg 5.1+ recommended, with libx264;
libx265 for HEVC sources).

## Requirements

* C++17 compiler (MSVC 2019+, GCC 9+, Clang / Apple Clang)
* CMake 3.21+
* Qt 6.2+ (Widgets)
* FFmpeg

## Build

### Linux (Debian / Ubuntu)
```bash
sudo apt install build-essential cmake qt6-base-dev libgl1-mesa-dev ffmpeg
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/VidCut
```

### macOS
```bash
brew install cmake qt ffmpeg
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build -j
open build/VidCut.app
```

### Windows
Install Visual Studio 2022 (workload "Desktop development with C++"), Qt 6 (MSVC kit) and CMake.
Then, in the *x64 Native Tools Command Prompt for VS 2022*:
```bat
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:\Qt\6.7.2\msvc2019_64
cmake --build build
```
(Adjust the Qt path. With Visual Studio generator: `-G "Visual Studio 17 2022"` and `cmake --build build --config Release`.)

Qt Creator also works: open `CMakeLists.txt`.

## Where VidCut looks for FFmpeg

1. Folder chosen in File > Set FFmpeg Location...
2. Next to the application (also `ffmpeg/`, `bin/`; `Contents/Resources` on macOS)
3. `PATH`
4. macOS: `/opt/homebrew/bin`, `/usr/local/bin`, `/opt/local/bin`

## Packaging

* **Windows**: `windeployqt --release build\VidCut.exe`, copy `ffmpeg.exe` + `ffprobe.exe` next to it, zip.
* **macOS**: copy `ffmpeg` + `ffprobe` into `VidCut.app/Contents/MacOS/`, then `macdeployqt build/VidCut.app -dmg`.
* **Linux**: AppImage via `linuxdeploy` + `linuxdeploy-plugin-qt`.

## Version

Edit one line in `CMakeLists.txt`: `project(VidCut VERSION 1.0.0 LANGUAGES CXX)`.
