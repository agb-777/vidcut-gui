from __future__ import annotations

import os
import re
import shutil
import tempfile
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import List, Optional

from PySide6.QtCore import QObject, QThread, Signal

from ..core.cut_planner import CutPlan, make_plan
from ..core.encode_settings import EncodeSettings
from ..core.encoder_profile import build_encoder_args, fallback_encoder_args
from ..core.pipeline_builder import PipelineParams, build_pipeline
from ..core.time_utils import format_fixed, format_size, format_time
from . import media_probe
from .errors import CancelToken, CutCancelled, CutError
from .file_utils import replace_file
from .process_runner import ProcessRunner


# A TS "null packet" (PID 0x1FFF) is the standard padding; demuxers ignore it.
_TS_NULL_PACKET = b"\x47\x1f\xff\x10" + b"\xff" * 184
_TS_PACKET_SIZE = 188
_MIN_TS_BYTES = 16 * 1024


def _pad_tiny_ts(path: str) -> None:
    """ffmpeg auto-detects the format of every segment when concatenating. A segment of
    only a few small frames is just a handful of TS packets, which is too little for the
    MPEG-TS probe: it gets misdetected as "mpeg" and the merge fails ("matches no streams").
    Padding it with null packets makes detection reliable without changing its content."""
    try:
        size = os.path.getsize(path)
        if size >= _MIN_TS_BYTES or size % _TS_PACKET_SIZE != 0:
            return
        missing = -(-(_MIN_TS_BYTES - size) // _TS_PACKET_SIZE)
        with open(path, "ab") as f:
            f.write(_TS_NULL_PACKET * missing)
    except OSError:
        pass  # best effort; a real I/O problem will surface in the next ffmpeg step


@dataclass
class CutJob:
    input: str  # absolute path
    output: str  # absolute path (.mp4 / .mkv / .mov)
    start: float
    end: float
    ffmpeg: str  # full path to executables
    ffprobe: str
    settings: EncodeSettings = field(default_factory=EncodeSettings)


class SmartCutter(QThread):
    """Runs the whole smartcut pipeline in a background thread.

    Use it like any QThread: connect the signals, then call start().
    The signals are emitted from the worker thread; Qt queues them to the UI thread.
    `finished` (inherited from QThread) is emitted after run() returns, so it always
    arrives after succeeded / failed / cancelled.

    Implemented as a QThread subclass (no event loop in the worker), so
    `cutter.wait()` can never deadlock - e.g. when the window is closed mid-cut.
    """

    stage_changed = Signal(str, int, int)  # label, index (1-based), count
    progress = Signal(float, float, float)  # fraction, speed, eta seconds (< 0 = unknown)
    succeeded = Signal(str)  # summary
    failed = Signal(str)  # message
    cancelled = Signal()

    def __init__(self, job: CutJob, parent: Optional[QObject] = None) -> None:
        super().__init__(parent)
        self._job = job
        self._cancel = CancelToken()

    def request_cancel(self) -> None:
        """Thread-safe; may be called from the UI thread at any time."""
        self._cancel.cancel()

    # QThread entry point (runs in the worker thread)
    def run(self) -> None:
        try:
            self._execute()
        except CutCancelled:
            self.cancelled.emit()
        except CutError as e:
            self.failed.emit(str(e))
        except Exception as e:  # noqa: BLE001 - never let an exception kill the thread silently
            self.failed.emit(f"Unexpected error: {e}")

    # ------------------------------------------------------------------------------

    def _execute(self) -> None:
        job = self._job
        runner = ProcessRunner(self._cancel)

        # ---- 1. ffmpeg capability check ---------------------------------------------
        opt_raw = runner.capture(job.ffmpeg, ["-hide_banner", "-h", "long"])
        options_known = opt_raw.count(b"\n") > 20
        opt_text = opt_raw.decode("utf-8", "replace")

        def has_option(name: str) -> bool:
            if not options_known:
                return True
            return re.search(r"^\s*-" + re.escape(name) + r"\b", opt_text, re.MULTILINE) is not None

        if not has_option("fps_mode"):
            raise CutError("This ffmpeg is too old (option -fps_mode is missing). "
                           "Please upgrade ffmpeg.")

        # ---- 2. probe -----------------------------------------------------------------
        media = media_probe.probe(runner, job.ffprobe, job.input)
        v = media.video

        if v.rotation != 0 and not has_option("display_rotation"):
            raise CutError("This video is rotated, which requires a newer ffmpeg "
                           "(option -display_rotation is missing). Please upgrade ffmpeg.")

        if media.duration > 0 and job.start >= media.duration:
            raise CutError(f"Start ({format_time(job.start)}) is beyond the video duration "
                           f"({format_time(media.duration)}).")

        # ---- 3. scratch directory next to the output (removed on any exit) ---------------
        out_dir = os.path.dirname(os.path.abspath(job.output))
        try:
            tmp_dir = tempfile.mkdtemp(prefix=".vidcut_", dir=out_dir)
        except OSError as e:
            raise CutError(f"Cannot write to output directory: {e.strerror or e}") from e

        try:
            source = job.input
            if "mpegts" in media.format_name:
                # ffmpeg cannot seek reliably in MPEG-TS (.ts/.mts/.m2ts): it lands inside a
                # GOP, which gives empty or too short segments. Remux to MKV first (a pure
                # stream copy) and cut that instead.
                source = self._remux_to_mkv(runner, job, media, tmp_dir)
                media = media_probe.probe(runner, job.ffprobe, source)
                v = media.video

            # ---- 4. encoder + plan --------------------------------------------------------
            smart_enc = build_encoder_args(v, job.settings)
            # Codec not supported by smartcut -> full re-encode with libx264.
            enc_args: List[str] = smart_enc if smart_enc is not None else \
                fallback_encoder_args(v, job.settings)

            plan: Optional[CutPlan] = None
            if smart_enc is not None:
                pkts = media_probe.read_packets(runner, job.ffprobe, source, v.index,
                                                media.start_time)
                # Incomplete timestamps or no usable keyframe -> plan stays None and the
                # whole range is re-encoded.
                if pkts:
                    plan = make_plan(pkts, job.start, job.end)

            self._run_pipeline(runner, job, source, media, enc_args, plan, tmp_dir)
        finally:
            shutil.rmtree(tmp_dir, ignore_errors=True)

    def _progress_callback(self, total: float):
        def on_progress(cur: float, speed: float) -> None:
            frac = min(1.0, cur / total) if total > 0 else 0.0
            eta = max(0.0, (total - cur) / speed) if speed > 0 and total > 0 else -1.0
            self.progress.emit(frac, speed, eta)
        return on_progress

    def _remux_to_mkv(self, runner: ProcessRunner, job: CutJob, media, tmp_dir: str) -> str:
        """Stream-copies video + audio of an MPEG-TS file into a temporary MKV."""
        dest = f"{Path(tmp_dir).as_posix()}/source.mkv"
        # index 0 / count 0 = "preparation", shown without "Step x of y"
        self.stage_changed.emit("Preparing: converting MPEG-TS to MKV (no re-encoding)...", 0, 0)
        runner.run_ffmpeg(job.ffmpeg,
                          ["-i", job.input, "-map", "0:v", "-map", "0:a?", "-sn", "-dn",
                           "-c", "copy", "-f", "matroska", dest],
                          media.duration, self._progress_callback(media.duration))
        try:
            ok = os.path.getsize(dest) > 0
        except OSError:
            ok = False
        if not ok:
            raise CutError("Could not convert this MPEG-TS file to MKV (no output was produced).")
        return dest

    def _run_pipeline(self, runner: ProcessRunner, job: CutJob, source: str, media,
                      enc_args: List[str], plan: Optional[CutPlan], tmp_dir: str) -> None:
        pp = PipelineParams(
            input=source,
            tmp_dir=Path(tmp_dir).as_posix(),  # forward slashes; keeps a literal \ in POSIX names
            output_ext=os.path.splitext(job.output)[1].lower(),
            media=media,
            encoder_args=enc_args,
            plan=plan,
            start=job.start,
            end=job.end,
            settings=job.settings,
        )
        pipeline = build_pipeline(pp)

        try:
            with open(pipeline.concat_list_path, "w", encoding="utf-8", newline="\n") as f:
                f.write(pipeline.concat_list_content)
        except OSError as e:
            raise CutError(f"Cannot write temporary file: {e.strerror or e}") from e

        # ---- 5. run every step ---------------------------------------------------------
        started = time.monotonic()
        count = len(pipeline.steps)
        for i, st in enumerate(pipeline.steps, start=1):
            self.stage_changed.emit(st.label, i, count)

            runner.run_ffmpeg(job.ffmpeg, st.args, st.total, self._progress_callback(st.total))

            if st.check_output:
                try:
                    size = os.path.getsize(st.check_output)
                except OSError:
                    size = 0
                if size > 0:
                    _pad_tiny_ts(st.check_output)
                if size == 0:
                    raise CutError(
                        f"ffmpeg produced an empty segment ({os.path.basename(st.check_output)}). "
                        "Seeking in this input is probably unreliable (common with .ts/mpegts). "
                        "Remux it first, e.g.: ffmpeg -i input.ts -c copy input.mkv")

        # ---- 6. move result into place ---------------------------------------------------
        if not os.path.isfile(pipeline.final_tmp_path):
            raise CutError("ffmpeg finished but did not produce an output file.")
        err = replace_file(pipeline.final_tmp_path, job.output)
        if err:
            raise CutError(err)

        # The result is already in place: failing to read its duration (or a cancel that
        # arrives at the very last moment) must not turn a finished cut into an error.
        try:
            out_dur = media_probe.read_duration(runner, job.ffprobe, job.output)
        except (CutError, CutCancelled):
            out_dur = 0.0
        target = pipeline.e_eff - pipeline.s_eff
        size = os.path.getsize(job.output)
        elapsed = time.monotonic() - started

        summary = f"Output:    {os.path.normpath(job.output)}\n"
        if out_dur > 0:
            summary += f"Duration:  {format_time(out_dur)} (target {format_time(target)})\n"
        summary += f"Size:      {format_size(size)}\nTime:      {format_fixed(elapsed, 1)}s"
        self.succeeded.emit(summary)
