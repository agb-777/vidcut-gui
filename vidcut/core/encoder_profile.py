from __future__ import annotations

from typing import List, Optional

from .encode_settings import EncodeSettings
from .media_info import VideoInfo

_H264_PROFILES = {
    "baseline": "baseline",
    "constrained baseline": "baseline",
    "main": "main",
    "high": "high",
    "high 10": "high10",
    "high 4:2:2": "high422",
    "high 4:4:4 predictive": "high444",
}

_H265_PROFILES = {
    "main": "main",
    "main 10": "main10",
    "main still picture": "main",
}


def build_encoder_args(v: VideoInfo, s: EncodeSettings) -> Optional[List[str]]:
    """Encoder arguments that match the source stream (codec, profile, level, pixel
    format, colour info) so re-encoded edges can be concatenated with the
    stream-copied middle. Returns None if the codec is not supported by smartcut."""
    profile = v.profile.lower()

    if v.codec_name == "h264":
        args = ["-c:v", "libx264", "-preset", s.preset, "-crf", str(s.crf)]
        if profile in _H264_PROFILES:
            args += ["-profile:v", _H264_PROFILES[profile]]
        if v.level > 0:
            args += ["-level:v", f"{v.level // 10}.{v.level % 10}"]
        args += ["-x264-params", "open-gop=0"]
    elif v.codec_name == "hevc":
        args = ["-c:v", "libx265", "-preset", s.preset, "-crf", str(s.crf)]
        x265 = "open-gop=0:repeat-headers=1:log-level=error"
        if profile in _H265_PROFILES:
            x265 += ":profile=" + _H265_PROFILES[profile]
        args += ["-x265-params", x265]
    else:
        return None

    if v.pix_fmt:
        args += ["-pix_fmt", v.pix_fmt]
    for value, option in (
        (v.color_space, "-colorspace"),
        (v.color_primaries, "-color_primaries"),
        (v.color_transfer, "-color_trc"),
        (v.color_range, "-color_range"),
    ):
        if value and value != "unknown":
            args += [option, value]
    return args


def fallback_encoder_args(v: VideoInfo, s: EncodeSettings) -> List[str]:
    """Full re-encode with libx264 for codecs smartcut cannot handle."""
    return ["-c:v", "libx264", "-preset", s.preset, "-crf", str(s.crf),
            "-pix_fmt", v.pix_fmt or "yuv420p"]
