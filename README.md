# VidCut (Python / PySide6)

Fast, lossless-where-possible video cutter. Python 3 + PySide6 port of the C++/Qt VidCut.

Keyframe-aligned parts are **stream-copied** without re-encoding; only the cut edges are
re-encoded with settings matched to the source (codec, profile, level, pixel format, colour info),
then everything is merged. Codecs other than H.264/HEVC fall back to a full libx264 re-encode.

## Run

Requires Python 3.9+ and FFmpeg (`ffmpeg` + `ffprobe`).

```bash
python -m venv .venv
source .venv/bin/activate        # Windows: .venv\Scripts\activate
pip install -r requirements.txt
python run_vidcut.py             # or: python -m vidcut
```

Optional: `pip install .` installs a `vidcut` launcher.

### FFmpeg lookup order
1. Folder chosen in **File > Set FFmpeg Location...**
2. Next to the app (also `./ffmpeg`, `./bin`, macOS `Resources`)
3. `PATH`
4. macOS: Homebrew / MacPorts locations

## Tests

```bash
python -m unittest discover -s tests -v
```
`test_core` needs nothing; `test_engine` / `test_gui_smoke` need FFmpeg (skipped if missing)
and run Qt headless (`QT_QPA_PLATFORM=offscreen`).

## Build a standalone app

PyInstaller cannot cross-compile: build on each target OS (or use a CI matrix).

```bash
pip install pyinstaller
pyinstaller --noconfirm --windowed --name VidCut run_vidcut.py
```
Then copy `ffmpeg` / `ffprobe` next to the produced executable (`dist/VidCut/`, on macOS inside
`VidCut.app/Contents/MacOS/` or `Contents/Resources/`) so users do not need to install FFmpeg.

## Layout

```
vidcut/core/     pure logic, no Qt (keyframes, cut plan, encoder args, ffmpeg pipeline)
vidcut/engine/   subprocess runner, ffprobe wrappers, settings, SmartCutter worker thread
vidcut/ui/       main window, settings panel, about dialog
tests/
```
Version is defined in one place: `vidcut/appinfo.py`.
