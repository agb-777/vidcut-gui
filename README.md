# VidCut

A small, fast video cutter with a plain white desktop GUI (Qt 6 Widgets, C++17).
It is a C++ rewrite of `smartcut.py`: the keyframe-aligned middle of your clip is
**copied without re-encoding**, and only the short edges are re-encoded. Runs on
Windows, Linux and macOS from one code base.

* Preset, CRF and audio bitrate are chosen in the UI (saved between runs, **Reset to Defaults** button).
* Defaults are identical to the Python script: `ultrafast`, CRF `20`, `192k`.
* The application version is defined **once**, in `CMakeLists.txt` (`project(VidCut VERSION ...)`).

VidCut calls the external `ffmpeg` and `ffprobe` programs, so FFmpeg must be installed
(or placed next to the app, see "Where VidCut looks for FFmpeg").

## Project layout

```
VidCut/
├─ CMakeLists.txt
├─ src/
│  ├─ main.cpp                    entry point + light theme
│  ├─ core/        (pure C++17, no Qt, unit-tested)
│  │  ├─ AppInfo.h                name/version (injected by CMake)
│  │  ├─ MediaInfo.h              video stream / media info structs
│  │  ├─ NumParse.*               locale-independent number parsing
│  │  ├─ TimeUtils.*              HH:MM:SS parse/format, size formatting
│  │  ├─ EncodeSettings.*         preset / CRF / audio bitrate + validation
│  │  ├─ EncoderProfile.*         encoder args matching the source stream
│  │  ├─ KeyframeAnalyzer.*       ffprobe packet parsing, closed keyframes
│  │  ├─ CutPlanner.*             the A (re-encode) / B (copy) / C (re-encode) plan
│  │  ├─ PlanFormatter.*          rows for the plan table
│  │  └─ PipelineBuilder.*        every ffmpeg command line of a cut
│  ├─ engine/      (Qt, no widgets)
│  │  ├─ ProcessRunner.*          QProcess wrapper, progress parsing, cancel
│  │  ├─ FfmpegLocator.*          finds ffmpeg/ffprobe on every OS
│  │  ├─ MediaProbe.*             ffprobe JSON / packets / duration
│  │  ├─ SmartCutter.*            worker: runs the whole pipeline in a thread
│  │  ├─ SettingsStore.*          QSettings persistence
│  │  ├─ FileUtils.*              safe move/replace of the output file
│  │  └─ CutErrors.h              CutError, CutCancelled, CancelToken
│  └─ ui/
│     ├─ MainWindow.*             main window
│     ├─ SettingsPanel.*          Encoding Settings box
│     ├─ PlanTableWidget.*        plan table
│     └─ AboutDialog.*            Help > About
└─ tests/test_core.cpp            unit tests for core/
```

## Requirements

* A C++17 compiler (MSVC 2019+/MinGW, GCC 9+, Clang/Apple Clang)
* CMake 3.21+
* Qt 6.2+ (Widgets module)
* FFmpeg (with `ffmpeg` **and** `ffprobe`), recent enough to support `-fps_mode`
  (FFmpeg 5.1+ recommended). `libx264` is required; `libx265` for HEVC sources.

## Build

### Linux (Debian / Ubuntu)

```bash
sudo apt update
sudo apt install build-essential cmake qt6-base-dev libgl1-mesa-dev ffmpeg

cd VidCut
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/VidCut
```

Fedora: `sudo dnf install gcc-c++ cmake qt6-qtbase-devel mesa-libGL-devel` (FFmpeg full build from RPM Fusion).
Arch: `sudo pacman -S base-devel cmake qt6-base ffmpeg`.

### macOS

```bash
brew install cmake qt ffmpeg

cd VidCut
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build -j
open build/VidCut.app
```

### Windows

1. Install **Visual Studio 2022** (workload "Desktop development with C++") or Build Tools.
2. Install **Qt 6** with the Qt Online Installer (select *MSVC 2022 64-bit*), and **CMake**.
3. Download an FFmpeg build (e.g. "release essentials" from gyan.dev) and keep `ffmpeg.exe` + `ffprobe.exe`.
4. Open *x64 Native Tools Command Prompt for VS 2022*:

```bat
cd VidCut
cmake -S . -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:\Qt\6.7.2\msvc2019_64
cmake --build build
```

(Adjust the Qt path/version. Without Ninja use `-G "Visual Studio 17 2022"` and
`cmake --build build --config Release`; the exe is then in `build\Release\`.)

Qt Creator also works on all three systems: *File > Open File or Project...* and select `CMakeLists.txt`.

## Run the unit tests (no Qt needed)

```bash
cmake -S . -B build-tests -DVIDCUT_BUILD_GUI=OFF -DVIDCUT_BUILD_TESTS=ON
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```

## Where VidCut looks for FFmpeg

1. Folder chosen in **File > Set FFmpeg Location...**
2. Next to the application (also `ffmpeg/` and `bin/` subfolders; `Contents/Resources` on macOS)
3. `PATH`
4. macOS: `/opt/homebrew/bin`, `/usr/local/bin`, `/opt/local/bin`

## Packaging

* **Windows**: run `windeployqt --release build\VidCut.exe`, copy `ffmpeg.exe` and `ffprobe.exe` into the same folder, zip it (or build an installer with Inno Setup / NSIS).
* **macOS**: `macdeployqt build/VidCut.app -dmg`. To ship FFmpeg inside, copy `ffmpeg` and `ffprobe` to `VidCut.app/Contents/MacOS/` *before* running `macdeployqt`. Public distribution needs code signing + notarization (Apple Developer account).
* **Linux**: ship the binary + Qt libs as an AppImage (`linuxdeploy` + `linuxdeploy-plugin-qt`), or a `.deb` / Flatpak.

Note on licensing: Qt is LGPL (link dynamically). FFmpeg builds that include `libx264`/`libx265` are GPL, which matters if you *bundle* them.

## Changing the version

Edit one line in `CMakeLists.txt`:

```cmake
project(VidCut VERSION 1.1.0 LANGUAGES CXX)
```

Re-run CMake; the title bar, About dialog and macOS bundle update automatically.

## Troubleshooting

* **"FFmpeg not found"** – install FFmpeg or use *File > Set FFmpeg Location...*.
* **"This ffmpeg is too old (option -fps_mode is missing)"** – upgrade FFmpeg.
* **"ffmpeg produced an empty segment"** – the input is probably `.ts`/MPEG-TS; remux first:
  `ffmpeg -i input.ts -c copy input.mkv`.
* **CMake cannot find Qt** – pass `-DCMAKE_PREFIX_PATH=<path to your Qt 6 kit>`.
* The `.vidcut_*` scratch folder is created next to the output file and deleted automatically, even on error or cancel.
