from __future__ import annotations

import json
from typing import Optional

from PySide6.QtCore import QObject, QProcess, Signal

from ..core.num_parse import parse_double


class DurationProbe(QObject):
    """Reads the duration of a video in the background (does not block the UI).

    Starting a new probe aborts the previous one.
    """

    succeeded = Signal(str, float)  # path, duration seconds
    failed = Signal(str, str)  # path, message

    def __init__(self, parent: Optional[QObject] = None) -> None:
        super().__init__(parent)
        self._proc: Optional[QProcess] = None
        self._path = ""

    def abort(self) -> None:
        proc, self._proc = self._proc, None  # handlers ignore every process but the current one
        if proc is None:
            return
        if proc.state() != QProcess.ProcessState.NotRunning:
            proc.kill()
            proc.waitForFinished(2000)
        proc.deleteLater()

    def start(self, ffprobe: str, path: str) -> None:
        self.abort()
        self._path = path
        proc = QProcess(self)
        self._proc = proc
        proc.finished.connect(lambda code, status: self._on_finished(proc, code, status))
        proc.errorOccurred.connect(lambda error: self._on_error(proc, error))
        proc.start(ffprobe, ["-v", "error", "-show_entries",
                             "format=duration:stream=codec_type",
                             "-of", "json", path])

    def _on_error(self, proc: QProcess, error: QProcess.ProcessError) -> None:
        if proc is not self._proc:
            return
        if error != QProcess.ProcessError.FailedToStart:
            return  # other errors end in finished()
        path = self._path
        message = f"Cannot start ffprobe: {proc.errorString()}"
        self._proc = None
        proc.deleteLater()
        self.failed.emit(path, message)

    def _on_finished(self, proc: QProcess, exit_code: int, status: QProcess.ExitStatus) -> None:
        if proc is not self._proc:
            return  # result of an aborted probe
        path = self._path
        out = bytes(proc.readAllStandardOutput().data())
        self._proc = None
        proc.deleteLater()

        if status != QProcess.ExitStatus.NormalExit or exit_code != 0:
            self.failed.emit(path, "This file could not be read as a video.")
            return

        try:
            root = json.loads(out.decode("utf-8", "replace"))
        except ValueError:
            root = {}
        if not isinstance(root, dict):
            root = {}

        streams = root.get("streams")
        has_video = isinstance(streams, list) and any(
            isinstance(s, dict) and s.get("codec_type") == "video" for s in streams
        )
        if not has_video:
            self.failed.emit(path, "This file does not contain a video stream.")
            return

        fmt = root.get("format")
        raw = fmt.get("duration") if isinstance(fmt, dict) else None
        duration = 0.0
        if isinstance(raw, (int, float)) and not isinstance(raw, bool):
            duration = float(raw)
        elif isinstance(raw, str):
            parsed = parse_double(raw)
            duration = parsed if parsed is not None else 0.0
        if duration <= 0.0:
            self.failed.emit(path, "Could not read the duration of this video.")
            return
        self.succeeded.emit(path, duration)
