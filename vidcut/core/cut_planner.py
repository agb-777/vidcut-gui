from __future__ import annotations

from dataclasses import dataclass
from typing import List, Optional

from .keyframe_analyzer import Packet, closed_keyframe_indices

EPS = 0.0005


@dataclass
class CutPlan:
    """Smartcut plan:  [A: re-encode] [B: stream copy] [C: re-encode]

    A = s_eff .. k1   (start to first closed keyframe)
    B = k1 .. k2      (copied untouched)
    C = k2 .. e_eff   (last closed keyframe to end)
    """

    s_eff: float = 0.0
    e_eff: float = 0.0
    k1: float = 0.0
    k2: float = 0.0
    a_len: float = 0.0
    b_len: float = 0.0
    c_len: float = 0.0
    b_packets: Optional[int] = None  # number of video packets in B (None = until EOF)


def make_plan(pkts: List[Packet], start: float, end: float) -> Optional[CutPlan]:
    """Returns None when the range is too short or has no usable closed keyframe;
    the caller should then re-encode the whole range."""
    pts = sorted(p.pts for p in pkts)
    if len(pts) < 2:
        return None

    diffs = sorted(b - a for a, b in zip(pts, pts[1:]) if b > a)
    frame_dur = diffs[len(diffs) // 2] if diffs else 0.04  # median frame duration

    s_eff = next((p for p in pts if p >= start - EPS), None)
    if s_eff is None:
        return None

    e_found = next((p for p in pts if p >= end - EPS), None)
    to_eof = e_found is None
    e_eff = pts[-1] + frame_dur if to_eof else e_found
    if e_eff - s_eff < frame_dur * 0.5:
        return None

    closed = closed_keyframe_indices(pkts)
    if not closed:
        return None

    i1 = next((i for i in closed if pkts[i].pts >= s_eff - EPS), None)
    if i1 is None:
        return None
    k1 = pkts[i1].pts

    c_len = 0.0
    b_packets: Optional[int] = None
    if to_eof:
        k2 = e_eff
    else:
        i2 = next((i for i in reversed(closed) if pkts[i].pts <= e_eff + EPS), None)
        if i2 is None:
            return None
        if pkts[i2].pts - k1 < frame_dur * 1.5 or i2 <= i1:
            return None
        k2 = pkts[i2].pts
        b_packets = i2 - i1
        c_len = e_eff - k2

    if k1 >= e_eff - EPS:
        return None

    return CutPlan(
        s_eff=s_eff,
        e_eff=e_eff,
        k1=k1,
        k2=k2,
        a_len=max(0.0, k1 - s_eff),
        b_len=k2 - k1,
        c_len=max(0.0, c_len),
        b_packets=b_packets,
    )
