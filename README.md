# VidCut

A video cutter for Windows, macOS and Linux, built with Python, PySide6 and FFmpeg.

It cuts without re-encoding wherever it can. The part between two keyframes is copied as is,
and only the start and end edges are re-encoded, using settings matched to the source file.
The pieces are then merged into one file.

## Features

- Smart cut for H.264 and HEVC. Other codecs are fully re-encoded with libx264.
- Output as `.mp4`, `.mkv` or `.mov`.
- Adjustable preset, CRF and audio bitrate (audio is always re-encoded to AAC).
- Keeps rotation, pixel format and colour info of the source.
- Drag and drop a file into the window.
- Progress bar with speed and ETA, cancel at any time.
- Dark theme.

## Requirements

- Python 3.9 or newer
- FFmpeg with `ffmpeg` and `ffprobe`. It needs the `-fps_mode` option (FFmpeg 5.1+), and rotated
  videos also need `-display_rotation` (a newer build). VidCut tells you if either is missing.

## Run

```bash
python -m venv .venv
source .venv/bin/activate        # Windows: .venv\Scripts\activate
pip install -r requirements.txt
python run_vidcut.py
```

## Usage

1. Choose or drag in an input video.
2. Set Start and End (`HH:MM:SS`). The output file name is filled in for you, change it if needed.
3. Adjust the encoding settings if you want, then click **Cut**.

## FFmpeg lookup

VidCut looks for `ffmpeg` and `ffprobe` in this order:

1. The folder set in **File > Set FFmpeg Location...**
2. Next to the app (also `./ffmpeg` and `./bin`, and `Resources` on macOS)
3. `PATH`
4. Homebrew and MacPorts locations on macOS

## Notes

- **Time fields** use whole seconds (`HH:MM:SS`, up to 23:59:59). The duration shown next to
  End is an estimate, the real result can differ slightly because cuts snap to frames and keyframes.
- **MPEG-TS** (`.ts`, `.mts`, `.m2ts`) cannot be seeked reliably, so VidCut first remuxes the file
  to a temporary MKV (no re-encoding). This needs free disk space about the size of the input.
- While cutting, a hidden `.vidcut_XXXX` folder is created next to the output file. It is removed
  when the cut finishes or is cancelled.

## Author

[agb-777](https://github.com/agb-777) - https://github.com/agb-777/vidcut-gui
