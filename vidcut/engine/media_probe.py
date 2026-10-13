from __future__ import annotations

import json
import math
from typing import Any, List, Optional

from ..core.keyframe_analyzer import Packet, parse_packets
from ..core.media_info import MediaInfo
from ..core.num_parse import parse_double
from .errors import CutError
from .process_runner import ProcessRunner


def _is_number(v: Any) -> bool:
    return isinstance(v, (int, float)) and not isinstance(v, bool)


def json_to_double(v: Any) -> float:
    """ffprobe writes some numbers as JSON numbers, others as strings."""
    if _is_number(v):
        return float(v)
    if isinstance(v, str):
        d = parse_double(v)
        if d is not None:
            return d
    return 0.0


def _lround(x: float) -> int:
    """Round half away from zero (like C's lround)."""
    return int(math.copysign(math.floor(abs(x) + 0.5), x))


def _as_dict(v: Any) -> dict:
    return v if isinstance(v, dict) else {}


def _as_list(v: Any) -> list:
    return v if isinstance(v, list) else []


def _str(o: dict, key: str) -> str:
    v = o.get(key)
    return v if isinstance(v, str) else ""


def _rotation_of(stream: dict) -> int:
    """Display-matrix side data first, then the old "rotate" tag."""
    for sd in _as_list(stream.get("side_data_list")):
        sd = _as_dict(sd)
        if "rotation" in sd:
            r = sd["rotation"]
            if not _is_number(r) and not isinstance(r, str):
                return 0
            return _lround(json_to_double(r))
    tag = _as_dict(stream.get("tags")).get("rotate")
    if not _is_number(tag) and not isinstance(tag, str):
        return 0
    return -_lround(json_to_double(tag))


def probe(runner: ProcessRunner, ffprobe: str, path: str) -> MediaInfo:
    """Stream/format info of the first real video stream (cover art is ignored)."""
    out = runner.capture(ffprobe, ["-v", "error", "-print_format", "json",
                                   "-show_format", "-show_streams", path])
    try:
        root = json.loads(out.decode("utf-8", "replace"))
    except ValueError as e:
        raise CutError(f"Cannot parse ffprobe output: {e}") from e
    if not isinstance(root, dict):
        raise CutError("Cannot parse ffprobe output: unexpected JSON structure")

    mi = MediaInfo()
    found_video = False
    for s in _as_list(root.get("streams")):
        s = _as_dict(s)
        kind = s.get("codec_type")
        if kind == "audio":
            mi.has_audio = True
        attached_pic = _as_dict(s.get("disposition")).get("attached_pic", 0)
        is_cover = _is_number(attached_pic) and int(attached_pic) != 0
        if kind == "video" and not found_video and not is_cover:
            v = mi.video
            idx = s.get("index", 0)
            v.index = int(idx) if _is_number(idx) else 0
            v.codec_name = _str(s, "codec_name")
            v.profile = _str(s, "profile")
            v.pix_fmt = _str(s, "pix_fmt")
            level = s.get("level")
            v.level = int(level) if _is_number(level) else 0
            v.color_space = _str(s, "color_space")
            v.color_primaries = _str(s, "color_primaries")
            v.color_transfer = _str(s, "color_transfer")
            v.color_range = _str(s, "color_range")
            v.rotation = _rotation_of(s)
            found_video = True
    if not found_video:
        raise CutError("No video stream found in this file.")

    fmt = _as_dict(root.get("format"))
    mi.duration = json_to_double(fmt.get("duration"))
    mi.start_time = json_to_double(fmt.get("start_time"))
    return mi


def read_packets(runner: ProcessRunner, ffprobe: str, path: str, stream_index: int,
                 start_offset: float) -> Optional[List[Packet]]:
    """Video packet timestamps + keyframe flags. None if incomplete."""
    out = runner.capture(ffprobe, ["-v", "error", "-select_streams", str(stream_index),
                                   "-show_entries", "packet=pts_time,flags",
                                   "-of", "csv=p=0", path])
    return parse_packets(out.decode("utf-8", "replace"), start_offset)


def read_duration(runner: ProcessRunner, ffprobe: str, path: str) -> float:
    """Container duration of a finished file; <= 0 if unknown."""
    out = runner.capture(ffprobe, ["-v", "error", "-show_entries", "format=duration",
                                   "-of", "csv=p=0", path])
    d = parse_double(out.decode("utf-8", "replace").strip())
    return d if d is not None else 0.0
