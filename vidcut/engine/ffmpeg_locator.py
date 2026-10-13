from __future__ import annotations

import os
import shutil
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import List


@dataclass(frozen=True)
class FfmpegTools:
    ffmpeg: str = ""  # full path
    ffprobe: str = ""  # full path

    def is_valid(self) -> bool:
        return bool(self.ffmpeg and self.ffprobe)


def _which_in(name: str, directory: str) -> str:
    found = shutil.which(name, path=directory)  # honours PATHEXT (.exe) on Windows
    return os.path.abspath(found) if found else ""


def find_in_dir(directory: str) -> FfmpegTools:
    """Both tools must be in `directory`."""
    if not directory:
        return FfmpegTools()
    ff = _which_in("ffmpeg", directory)
    fp = _which_in("ffprobe", directory)
    if ff and fp:
        return FfmpegTools(ff, fp)
    return FfmpegTools()


def _application_dirs() -> List[str]:
    """Folders "next to the application", for both source runs and frozen builds."""
    dirs: List[str] = []
    if getattr(sys, "frozen", False):  # PyInstaller / Nuitka / py2app
        base = Path(sys.executable).resolve().parent
        meipass = getattr(sys, "_MEIPASS", None)
        if meipass:
            dirs.append(str(meipass))
    else:
        base = Path(__file__).resolve().parents[2]  # folder that contains the "vidcut" package
    dirs += [str(base), str(base / "ffmpeg"), str(base / "bin")]
    if sys.platform == "darwin":
        dirs += [str(base / ".." / "Resources"), str(base / ".." / "Resources" / "ffmpeg")]
    return dirs


def locate(custom_dir: str = "") -> FfmpegTools:
    """Finds ffmpeg + ffprobe on every OS. Search order:

    1. user-chosen folder (File > Set FFmpeg Location...)
    2. next to the application (and ./ffmpeg, ./bin, macOS Resources)
    3. PATH
    4. common macOS locations (Homebrew/MacPorts; Finder-launched apps get a minimal PATH)
    """
    dirs: List[str] = []
    if custom_dir:
        dirs.append(custom_dir)
    dirs += _application_dirs()
    dirs += [d for d in os.environ.get("PATH", "").split(os.pathsep) if d]
    if sys.platform == "darwin":
        dirs += ["/opt/homebrew/bin", "/usr/local/bin", "/opt/local/bin"]

    seen = set()
    for d in dirs:
        d = os.path.normpath(d)
        if d in seen:
            continue
        seen.add(d)
        tools = find_in_dir(d)
        if tools.is_valid():
            return tools

    # Last resort: tools living in different folders on PATH.
    ff = shutil.which("ffmpeg") or ""
    fp = shutil.which("ffprobe") or ""
    if ff and fp:
        return FfmpegTools(os.path.abspath(ff), os.path.abspath(fp))
    return FfmpegTools()
