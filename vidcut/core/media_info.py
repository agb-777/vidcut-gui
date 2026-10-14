from __future__ import annotations

from dataclasses import dataclass, field


@dataclass
class VideoInfo:
    """Properties of the first real video stream (filled from ffprobe output)."""

    index: int = 0
    codec_name: str = ""
    profile: str = ""  # as reported by ffprobe, e.g. "High"
    pix_fmt: str = ""
    level: int = 0
    color_space: str = ""
    color_primaries: str = ""
    color_transfer: str = ""
    color_range: str = ""
    rotation: int = 0  # degrees, from display matrix / rotate tag


@dataclass
class MediaInfo:
    video: VideoInfo = field(default_factory=VideoInfo)
    has_audio: bool = False
    duration: float = 0.0
    start_time: float = 0.0
    format_name: str = ""  # ffprobe format_name, e.g. "matroska,webm" or "mpegts"
