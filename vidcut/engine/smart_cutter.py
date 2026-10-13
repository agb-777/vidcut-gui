from __future__ import annotations

import os
import re
import shutil
import tempfile
import time
from dataclasses import dataclass, field
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

        # ---- 3. encoder + plan ----------------------------------------------------------
        smart_enc = build_encoder_args(v, job.settings)
        smart_ok = smart_enc is not None
        # Codec not supported by smartcut -> full re-encode with libx264.
        enc_args: List[str] = smart_enc if smart_enc is not None else \
            fallback_encoder_args(v, job.settings)

        plan: Optional[CutPlan] = None
        if smart_ok:
            pkts = media_probe.read_packets(runner, job.ffprobe, job.input, v.index,
                                            media.start_time)
            # Incomplete timestamps or no usable keyframe -> plan stays None and the
            # whole range is re-encoded.
            if pkts:
                plan = make_plan(pkts, job.start, job.end)

        # ---- 4. scratch directory next to the output (removed on any exit) ---------------
        out_dir = os.path.dirname(os.path.abspath(job.output))
        try:
            tmp_dir = tempfile.mkdtemp(prefix=".vidcut_", dir=out_dir)
        except OSError as e:
            raise CutError(f"Cannot write to output directory: {e.strerror or e}") from e

        try:
            self._run_pipeline(runner, job, media, enc_args, plan, tmp_dir)
        finally:
            shutil.rmtree(tmp_dir, ignore_errors=True)

    def _run_pipeline(self, runner: ProcessRunner, job: CutJob, media, enc_args: List[str],
                      plan: Optional[CutPlan], tmp_dir: str) -> None:
        pp = PipelineParams(
            input=job.input,
            tmp_dir=tmp_dir.replace("\\", "/"),
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

            total = st.total

            def on_progress(cur: float, speed: float, total: float = total) -> None:
                frac = min(1.0, cur / total) if total > 0 else 0.0
                eta = max(0.0, (total - cur) / speed) if speed > 0 and total > 0 else -1.0
                self.progress.emit(frac, speed, eta)

            runner.run_ffmpeg(job.ffmpeg, st.args, total, on_progress)

            if st.check_output:
                try:
                    size = os.path.getsize(st.check_output)
                except OSError:
                    size = 0
                if size == 0:
                    raise CutError(
                        f"ffmpeg produced an empty segment ({os.path.basename(st.check_output)}). "
                        "Seeking in this input is probably unreliable (common with .ts/mpegts). "
                        "Remux it first, e.g.: ffmpeg -i input.ts -c copy input.mkv")

        # ---- 6. move result into place ---------------------------------------------------
        err = replace_file(pipeline.final_tmp_path, job.output)
        if err:
            raise CutError(err)

        out_dur = media_probe.read_duration(runner, job.ffprobe, job.output)
        target = pipeline.e_eff - pipeline.s_eff
        size = os.path.getsize(job.output)
        elapsed = time.monotonic() - started

        summary = f"Output:    {os.path.normpath(job.output)}\n"
        if out_dur > 0:
            summary += f"Duration:  {format_time(out_dur)} (target {format_time(target)})\n"
        summary += f"Size:      {format_size(size)}\nTime:      {format_fixed(elapsed, 1)}s"
        self.succeeded.emit(summary)
