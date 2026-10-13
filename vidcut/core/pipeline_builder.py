from __future__ import annotations

from dataclasses import dataclass, field
from typing import List, Optional, Tuple

from .cut_planner import EPS, CutPlan
from .encode_settings import EncodeSettings
from .media_info import MediaInfo
from .time_utils import format_fixed, format_seconds


@dataclass
class PipelineParams:
    input: str  # absolute path of the source file
    tmp_dir: str  # scratch directory (forward slashes)
    output_ext: str  # lower-case with dot: ".mp4" / ".mkv" / ".mov"
    media: MediaInfo
    encoder_args: List[str]  # from build_encoder_args / fallback_encoder_args
    plan: Optional[CutPlan]  # None = re-encode the whole range
    start: float  # requested range (seconds)
    end: float
    settings: EncodeSettings


@dataclass
class FfmpegStep:
    """One ffmpeg invocation. The runner prepends:  -hide_banner -loglevel error -y"""

    label: str
    args: List[str]
    total: float = 0.0  # media seconds produced (for the progress bar)
    check_output: str = ""  # if set: must exist and be non-empty afterwards


@dataclass
class Pipeline:
    steps: List[FfmpegStep] = field(default_factory=list)  # in order; last = final mux
    concat_list_path: str = ""  # must be written before the mux step
    concat_list_content: str = ""
    final_tmp_path: str = ""  # result of the mux step (to be moved to the output)
    s_eff: float = 0.0  # effective range, used for the "target" duration
    e_eff: float = 0.0


def _escape_concat_path(path: str) -> str:
    return path.replace("'", "'\\''")


def build_pipeline(p: PipelineParams) -> Pipeline:
    pl = Pipeline()
    v = p.media.video
    vmap = f"0:{v.index}"
    codec = v.codec_name
    sec = format_seconds

    def tmp(name: str) -> str:
        return f"{p.tmp_dir}/{name}"

    # (path, duration) pairs for the concat list; duration < 0 means "not written".
    segments: List[Tuple[str, float]] = []

    if p.plan is None:
        # ---- whole range re-encoded ---------------------------------------
        s_eff = p.start
        e_eff = min(p.end, p.media.duration) if p.media.duration > 0.0 else p.end
        ts = tmp("full.ts")
        seek = max(0.0, s_eff - EPS)
        rel_end = e_eff - seek

        a = ["-noautorotate", "-ss", sec(seek), "-i", p.input,
             "-t", sec(rel_end + 1), "-vf", "trim=end=" + sec(rel_end),
             "-map", vmap, "-an"]
        a += p.encoder_args
        a += ["-fps_mode", "passthrough", "-f", "mpegts", ts]
        pl.steps.append(FfmpegStep("Re-encoding the whole range...", a, e_eff - s_eff, ts))
        segments.append((ts, -1.0))
    else:
        # ---- smartcut: A (re-encode) + B (copy) + C (re-encode) -----------
        c = p.plan
        s_eff = c.s_eff
        e_eff = c.e_eff

        if c.a_len > EPS:
            ts = tmp("a.ts")
            a = ["-noautorotate", "-ss", sec(max(0.0, s_eff - EPS)), "-i", p.input,
                 "-t", sec(c.a_len), "-map", vmap, "-an"]
            a += p.encoder_args
            a += ["-fps_mode", "passthrough", "-f", "mpegts", ts]
            pl.steps.append(FfmpegStep(
                f"[A] Re-encoding {format_fixed(c.a_len, 3)}s (start to first keyframe)...",
                a, c.a_len, ts))
            segments.append((ts, c.a_len))

        ts = tmp("b.ts")
        a = ["-ss", sec(c.k1 + EPS), "-i", p.input, "-map", vmap, "-an"]
        if c.b_packets is not None:
            a += ["-frames:v", str(c.b_packets)]
        a += ["-c:v", "copy"]
        if codec == "h264":
            a += ["-bsf:v", "h264_mp4toannexb"]
        elif codec == "hevc":
            a += ["-bsf:v", "hevc_mp4toannexb"]
        a += ["-avoid_negative_ts", "make_zero", "-f", "mpegts", ts]
        pl.steps.append(FfmpegStep(
            f"[B] Copying {format_fixed(c.b_len, 3)}s without re-encode...",
            a, c.b_len, ts))
        segments.append((ts, c.b_len))

        if c.c_len > EPS:
            ts = tmp("c.ts")
            a = ["-noautorotate", "-ss", sec(max(0.0, c.k2 - EPS)), "-i", p.input,
                 "-t", sec(c.c_len), "-map", vmap, "-an"]
            a += p.encoder_args
            a += ["-fps_mode", "passthrough", "-f", "mpegts", ts]
            pl.steps.append(FfmpegStep(
                f"[C] Re-encoding {format_fixed(c.c_len, 3)}s (last keyframe to end)...",
                a, c.c_len, ts))
            segments.append((ts, c.c_len))

    # ---- concat list --------------------------------------------------------
    pl.concat_list_path = tmp("list.txt")
    lines: List[str] = []
    for path, dur in segments:
        lines.append(f"file '{_escape_concat_path(path)}'")
        if dur >= 0.0:
            lines.append(f"duration {sec(dur)}")
    pl.concat_list_content = "\n".join(lines) + "\n"

    # ---- audio (always re-encoded to AAC) -----------------------------------
    audio_path = ""
    if p.media.has_audio:
        audio_path = tmp("audio.mkv")
        pl.steps.append(FfmpegStep(
            "Re-encoding audio (AAC)...",
            ["-ss", sec(max(0.0, s_eff)), "-i", p.input,
             "-t", sec(e_eff - s_eff), "-map", "0:a", "-vn", "-sn",
             "-c:a", "aac", "-b:a", p.settings.audio_bitrate, audio_path],
            e_eff - s_eff, ""))

    # ---- final mux -----------------------------------------------------------
    mux: List[str] = []
    if v.rotation != 0:
        mux += ["-display_rotation", str(v.rotation)]
    mux += ["-f", "concat", "-safe", "0", "-i", pl.concat_list_path]
    if audio_path:
        mux += ["-i", audio_path]
    mux += ["-map", "0:v:0"]
    if audio_path:
        mux += ["-map", "1:a"]
    mux += ["-c", "copy"]
    if p.output_ext in (".mp4", ".mov"):
        if codec == "hevc":
            mux += ["-tag:v", "hvc1"]
        mux += ["-movflags", "+faststart"]
    pl.final_tmp_path = tmp("output" + p.output_ext)
    mux += ["-avoid_negative_ts", "make_zero", pl.final_tmp_path]
    pl.steps.append(FfmpegStep("Merging all segments...", mux, e_eff - s_eff, ""))

    pl.s_eff = s_eff
    pl.e_eff = e_eff
    return pl
