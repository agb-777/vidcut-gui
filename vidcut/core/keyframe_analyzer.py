from __future__ import annotations

import math
from dataclasses import dataclass
from typing import List, Optional

from .num_parse import parse_double


@dataclass(frozen=True)
class Packet:
    pts: float  # seconds, already shifted by the container start time
    keyframe: bool


def parse_packets(csv: str, start_offset: float) -> Optional[List[Packet]]:
    """Parses `ffprobe -show_entries packet=pts_time,flags -of csv=p=0` output.

    Returns None if the output is empty or any timestamp is missing/invalid.
    """
    packets: List[Packet] = []
    for raw in csv.split("\n"):
        line = raw.strip(" \t\r\n")  # also handles "\r\n" from Windows ffprobe
        if not line:
            continue
        pts_field, sep, flags_field = line.partition(",")
        if not sep:
            continue  # fewer than 2 fields
        flags_field = flags_field.split(",", 1)[0]

        pts = parse_double(pts_field)
        if pts is None:
            return None
        packets.append(Packet(pts - start_offset, "K" in flags_field))

    return packets or None


def closed_keyframe_indices(pkts: List[Packet]) -> List[int]:
    """Indices of keyframes that are "closed": no packet before it (in decode order)
    has a later PTS, and no packet after it has an earlier PTS. Only such keyframes
    are safe cut points for stream copy."""
    n = len(pkts)
    inf = math.inf

    prefix_max = [-inf] * n  # max PTS of all packets BEFORE i
    m = -inf
    for i in range(n):
        prefix_max[i] = m
        if pkts[i].pts > m:
            m = pkts[i].pts

    suffix_min = [inf] * n  # min PTS of all packets AFTER i
    m = inf
    for i in range(n - 1, -1, -1):
        suffix_min[i] = m
        if pkts[i].pts < m:
            m = pkts[i].pts

    return [
        i for i in range(n)
        if pkts[i].keyframe and prefix_max[i] < pkts[i].pts < suffix_min[i]
    ]
