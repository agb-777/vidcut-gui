from __future__ import annotations

import math
import os
from typing import Optional

from PySide6.QtCore import QMimeData, Qt, QTime, QTimer, Slot
from PySide6.QtGui import QAction, QCloseEvent, QDragEnterEvent, QDropEvent
from PySide6.QtWidgets import (
    QAbstractSpinBox, QFileDialog, QGridLayout, QGroupBox, QHBoxLayout, QLabel, QLineEdit,
    QMainWindow, QMessageBox, QProgressBar, QPushButton, QTimeEdit, QVBoxLayout, QWidget,
)

from .. import appinfo
from ..core.encode_settings import validate_settings
from ..engine import ffmpeg_locator
from ..engine.duration_probe import DurationProbe
from ..engine.settings_store import SettingsStore
from ..engine.smart_cutter import CutJob, SmartCutter
from .about_dialog import AboutDialog
from .settings_panel import SettingsPanel

OUTPUT_EXTS = ("mp4", "mkv", "mov")
_APP_TITLE = appinfo.NAME


# ---------------------------------------------------------------------------------
# small helpers
# ---------------------------------------------------------------------------------

def format_eta(seconds: float) -> str:
    s = int(math.floor(seconds + 0.5))
    h, m, sec = s // 3600, (s // 60) % 60, s % 60
    if h > 0:
        return f"{h}:{m:02d}:{sec:02d}"
    return f"{m}:{sec:02d}"


def to_seconds(t: QTime) -> int:
    return t.hour() * 3600 + t.minute() * 60 + t.second()


def from_seconds(s: int) -> QTime:
    s = max(0, min(s, 24 * 3600 - 1))  # the HH:MM:SS editor covers 00:00:00 - 23:59:59
    return QTime(s // 3600, (s // 60) % 60, s % 60)


def format_hms(seconds: float) -> str:
    s = int(math.floor(max(0.0, seconds) + 0.5))
    return f"{s // 3600:02d}:{(s // 60) % 60:02d}:{s % 60:02d}"


def suffix_of(path: str) -> str:
    return os.path.splitext(path)[1].lstrip(".").lower()


def dropped_file_path(mime: Optional[QMimeData]) -> str:
    """First regular file in a drag & drop payload (folders and web links are ignored)."""
    if mime is None or not mime.hasUrls():
        return ""
    for url in mime.urls():
        if not url.isLocalFile():
            continue
        path = url.toLocalFile()
        if os.path.isfile(path):
            return path
    return ""


def make_time_edit(parent: QWidget) -> QTimeEdit:
    e = QTimeEdit(QTime(0, 0, 0), parent)
    e.setDisplayFormat("HH:mm:ss")  # typing 2 digits jumps to the next block
    e.setButtonSymbols(QAbstractSpinBox.ButtonSymbols.NoButtons)
    e.setAlignment(Qt.AlignmentFlag.AlignCenter)
    e.setMinimumWidth(90)
    return e


def native(path: str) -> str:
    return os.path.normpath(path)


# ---------------------------------------------------------------------------------
# window
# ---------------------------------------------------------------------------------

class MainWindow(QMainWindow):
    def __init__(self, parent: Optional[QWidget] = None) -> None:
        super().__init__(parent)
        self.setWindowTitle(f"{appinfo.NAME} v{appinfo.VERSION}")

        self._store = SettingsStore()

        self._cutter: Optional[SmartCutter] = None
        self._loaded_path = ""  # file whose duration we are reading / have read
        self._input_duration = 0.0  # seconds; 0 = nothing loaded yet
        self._probing = False
        self._cancel_requested = False
        self._output_autofilled = True  # output field was generated from the input name

        self._build_ui()
        self._build_menus()

        self._probe = DurationProbe(self)
        self._probe.succeeded.connect(self._on_probe_succeeded)
        self._probe.failed.connect(self._on_probe_failed)

        self._settings_panel.set_settings(self._store.load_encode_settings())
        geometry = self._store.window_geometry()
        if not geometry.isEmpty():
            self.restoreGeometry(geometry)
        else:
            self.resize(620, 430)

        # Drag & drop: accepted by the window; text fields must not swallow the drop.
        self.setAcceptDrops(True)
        for e in self.findChildren(QLineEdit):
            e.setAcceptDrops(False)
        for s in self.findChildren(QAbstractSpinBox):
            s.setAcceptDrops(False)

        self._update_controls()
        QTimer.singleShot(0, self._check_ffmpeg_on_startup)

    # -- construction ---------------------------------------------------------------

    def _build_ui(self) -> None:
        central = QWidget(self)
        root = QVBoxLayout(central)

        # --- files ---
        files = QGroupBox("Files", central)
        fg = QGridLayout(files)
        self._input_edit = QLineEdit(files)
        self._input_edit.setReadOnly(True)  # chosen with Browse or drag & drop
        self._input_browse = QPushButton("Browse...", files)
        self._output_edit = QLineEdit(files)
        self._output_browse = QPushButton("Browse...", files)
        fg.addWidget(QLabel("Input:", files), 0, 0)
        fg.addWidget(self._input_edit, 0, 1)
        fg.addWidget(self._input_browse, 0, 2)
        fg.addWidget(QLabel("Output:", files), 1, 0)
        fg.addWidget(self._output_edit, 1, 1)
        fg.addWidget(self._output_browse, 1, 2)
        root.addWidget(files)

        self._input_browse.clicked.connect(self._browse_input)
        self._output_browse.clicked.connect(self._browse_output)
        self._output_edit.textEdited.connect(self._on_output_edited)

        # --- time range ---
        rng = QGroupBox("Time Range", central)
        rg = QHBoxLayout(rng)
        self._start_edit = make_time_edit(rng)
        self._end_edit = make_time_edit(rng)
        rg.addWidget(QLabel("Start:", rng))
        rg.addWidget(self._start_edit)
        rg.addSpacing(16)
        rg.addWidget(QLabel("End:", rng))
        rg.addWidget(self._end_edit)
        rg.addSpacing(16)
        rg.addWidget(QLabel("Duration:", rng))
        self._duration_label = QLabel("--:--:--", rng)
        self._duration_label.setMinimumWidth(70)
        self._duration_label.setToolTip(
            "Estimated length of the result (End is limited to the video length).")
        rg.addWidget(self._duration_label)
        rg.addStretch(1)
        self._start_edit.timeChanged.connect(self._update_duration)
        self._end_edit.timeChanged.connect(self._update_duration)
        root.addWidget(rng)

        # --- settings ---
        self._settings_panel = SettingsPanel(central)
        root.addWidget(self._settings_panel)

        # --- actions + progress ---
        buttons = QHBoxLayout()
        self._cut_button = QPushButton("Cut", central)
        self._cut_button.setDefault(True)
        self._cut_button.setMinimumWidth(110)
        self._cancel_button = QPushButton("Cancel", central)
        self._cancel_button.setMinimumWidth(110)
        buttons.addWidget(self._cut_button)
        buttons.addWidget(self._cancel_button)
        buttons.addStretch(1)
        root.addLayout(buttons)
        self._cut_button.clicked.connect(self._start_cut)
        self._cancel_button.clicked.connect(self._cancel_cut)

        self._stage_label = QLabel("Ready", central)
        self._progress = QProgressBar(central)
        self._progress.setRange(0, 1000)
        self._progress.setValue(0)
        self._speed_label = QLabel(central)
        root.addWidget(self._stage_label)
        root.addWidget(self._progress)
        root.addWidget(self._speed_label)
        root.addStretch(1)

        self.setCentralWidget(central)

    def _build_menus(self) -> None:
        file_menu = self.menuBar().addMenu("&File")
        set_ff = file_menu.addAction("Set &FFmpeg Location...")
        set_ff.triggered.connect(self._choose_ffmpeg_location)
        file_menu.addSeparator()
        quit_action = file_menu.addAction("E&xit")
        quit_action.triggered.connect(self.close)

        help_menu = self.menuBar().addMenu("&Help")
        about = help_menu.addAction(f"&About {appinfo.NAME}")
        about.setMenuRole(QAction.MenuRole.AboutRole)
        about.triggered.connect(self._show_about)

    @Slot()
    def _update_duration(self, *_args) -> None:
        start = to_seconds(self._start_edit.time())
        end = float(to_seconds(self._end_edit.time()))
        if self._input_duration > 0.0:
            end = min(end, self._input_duration)  # the cutter clamps to the real end
        length = end - start
        self._duration_label.setText(format_hms(length) if length > 0 else "--:--:--")

    def _update_controls(self) -> None:
        running = self._cutter is not None
        self._input_browse.setEnabled(not running)
        self._output_edit.setEnabled(not running)
        self._output_browse.setEnabled(not running)
        self._start_edit.setEnabled(not running)
        self._end_edit.setEnabled(not running)
        self._settings_panel.setEnabled(not running)
        self._cut_button.setEnabled(not running and not self._probing and self._input_duration > 0.0)
        self._cancel_button.setEnabled(running and not self._cancel_requested)

    @staticmethod
    def _default_output_for(input_path: str) -> str:
        folder = os.path.dirname(os.path.abspath(input_path))
        base = os.path.splitext(os.path.basename(input_path))[0]
        ext = suffix_of(input_path)
        if ext not in OUTPUT_EXTS:
            ext = "mp4"
        return os.path.join(folder, f"{base}_cut.{ext}")

    def _warn(self, message: str) -> None:
        QMessageBox.warning(self, _APP_TITLE, message)

    # -- loading an input file (Browse or drag & drop) -----------------------------------

    @Slot()
    def _browse_input(self) -> None:
        path, _ = QFileDialog.getOpenFileName(
            self, "Select Input Video", self._store.last_dir("input"),
            "Video files (*.mp4 *.mkv *.mov *.avi *.webm *.ts *.m4v *.flv *.wmv *.mts *.m2ts);;"
            "All files (*)")
        if path:
            self._load_input(path)

    def dragEnterEvent(self, event: QDragEnterEvent) -> None:  # noqa: N802 (Qt API name)
        if self._cutter is None and dropped_file_path(event.mimeData()):
            event.acceptProposedAction()

    def dropEvent(self, event: QDropEvent) -> None:  # noqa: N802
        if self._cutter is not None:
            return
        path = dropped_file_path(event.mimeData())
        if not path:
            return
        event.acceptProposedAction()
        self._load_input(path)

    def _load_input(self, path: str) -> None:
        tools = ffmpeg_locator.locate(self._store.ffmpeg_dir())
        if not tools.is_valid():
            QMessageBox.warning(
                self, "FFmpeg not found",
                "ffmpeg / ffprobe not found.\nUse File > Set FFmpeg Location... to choose them.")
            return

        absolute = os.path.abspath(path)
        self._loaded_path = absolute
        self._input_duration = 0.0
        self._input_edit.setText(native(absolute))
        self._store.set_last_dir("input", os.path.dirname(absolute))
        if self._output_autofilled or not self._output_edit.text().strip():
            self._output_edit.setText(native(self._default_output_for(absolute)))
            self._output_autofilled = True
        self._start_edit.setTime(QTime(0, 0, 0))
        self._end_edit.setTime(QTime(0, 0, 0))
        self._progress.setValue(0)
        self._speed_label.clear()
        self._stage_label.setText("Reading video info...")
        self._update_duration()

        self._probing = True
        self._update_controls()
        self._probe.start(tools.ffprobe, absolute)

    def _clear_input(self) -> None:
        self._loaded_path = ""
        self._input_duration = 0.0
        self._input_edit.clear()
        if self._output_autofilled:
            self._output_edit.clear()
        self._start_edit.setTime(QTime(0, 0, 0))
        self._end_edit.setTime(QTime(0, 0, 0))
        self._update_duration()

    @Slot(str, float)
    def _on_probe_succeeded(self, path: str, duration: float) -> None:
        if path != self._loaded_path:
            return  # result of an older file
        self._probing = False
        self._input_duration = duration
        # Round up to whole seconds: the cutter clamps the end to the real end of the video.
        self._end_edit.setTime(from_seconds(int(math.ceil(duration - 0.001))))
        self._stage_label.setText("Ready")
        self._update_duration()
        self._update_controls()

    @Slot(str, str)
    def _on_probe_failed(self, path: str, message: str) -> None:
        if path != self._loaded_path:
            return
        self._probing = False
        self._clear_input()
        self._stage_label.setText("Ready")
        self._update_controls()
        self._warn(message)

    # -- output / ffmpeg location / about ------------------------------------------------

    @Slot(str)
    def _on_output_edited(self, _text: str) -> None:
        self._output_autofilled = False

    @Slot()
    def _browse_output(self) -> None:
        start = self._output_edit.text().strip()
        if not start:
            start = self._store.last_dir("output")
        path, _ = QFileDialog.getSaveFileName(
            self, "Select Output File", start, "Video files (*.mp4 *.mkv *.mov)", "",
            QFileDialog.Option.DontConfirmOverwrite)  # we ask ourselves when starting
        if not path:
            return
        self._output_edit.setText(native(path))
        self._output_autofilled = False
        self._store.set_last_dir("output", os.path.dirname(os.path.abspath(path)))

    @Slot()
    def _check_ffmpeg_on_startup(self) -> None:
        if ffmpeg_locator.locate(self._store.ffmpeg_dir()).is_valid():
            return
        QMessageBox.warning(
            self, "FFmpeg not found",
            f"{appinfo.NAME} needs ffmpeg and ffprobe, but could not find them.\n\n"
            "Install FFmpeg, or use File > Set FFmpeg Location... to choose the folder "
            "that contains both ffmpeg and ffprobe.")

    @Slot()
    def _choose_ffmpeg_location(self) -> None:
        directory = QFileDialog.getExistingDirectory(
            self, "Select the folder containing ffmpeg and ffprobe", self._store.ffmpeg_dir())
        if not directory:
            return
        if not ffmpeg_locator.find_in_dir(directory).is_valid():
            QMessageBox.warning(self, "FFmpeg not found",
                                "That folder does not contain both ffmpeg and ffprobe.")
            return
        self._store.set_ffmpeg_dir(directory)

    @Slot()
    def _show_about(self) -> None:
        AboutDialog(self).exec()

    # -- cutting -----------------------------------------------------------------------------

    @Slot()
    def _start_cut(self) -> None:
        if self._cutter is not None or self._probing:
            return

        tools = ffmpeg_locator.locate(self._store.ffmpeg_dir())
        if not tools.is_valid():
            self._warn("ffmpeg / ffprobe not found.\n"
                       "Use File > Set FFmpeg Location... to choose them.")
            return

        input_path = self._loaded_path
        if not input_path or self._input_duration <= 0.0 or not os.path.isfile(input_path):
            self._warn("Please select an input video first.")
            return

        start = to_seconds(self._start_edit.time())
        end = to_seconds(self._end_edit.time())
        if start >= end:
            self._warn("Start time must be earlier than end time.")
            return
        if start >= self._input_duration:
            self._warn("Start time is beyond the end of the video.")
            return

        out_text = self._output_edit.text().strip()
        if not out_text:
            self._warn("Please choose an output file.")
            return
        output = os.path.abspath(out_text)
        if not suffix_of(output):  # no extension typed: reuse the input's (or .mp4)
            ext = suffix_of(input_path)
            if ext not in OUTPUT_EXTS:
                ext = "mp4"
            output = output.rstrip(".") + "." + ext
            self._output_edit.setText(native(output))
        if suffix_of(output) not in OUTPUT_EXTS:
            self._warn("Output extension must be .mp4, .mkv or .mov.")
            return
        out_folder = os.path.dirname(output)
        if not os.path.isdir(out_folder):
            self._warn(f"Output folder does not exist:\n{native(out_folder)}")
            return
        if os.path.exists(output):
            try:
                same = os.path.samefile(input_path, output)
            except OSError:
                same = False
            if same:
                self._warn("Output must not be the same file as the input.")
                return
            answer = QMessageBox.question(
                self, "Overwrite file?", f"{native(output)}\nalready exists. Overwrite?",
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
                QMessageBox.StandardButton.No)
            if answer != QMessageBox.StandardButton.Yes:
                return

        enc = self._settings_panel.settings()
        problem = validate_settings(enc)
        if problem:
            self._warn(problem)
            return
        self._store.save_encode_settings(enc)

        job = CutJob(input=input_path, output=output, start=float(start), end=float(end),
                     ffmpeg=tools.ffmpeg, ffprobe=tools.ffprobe, settings=enc)

        self._progress.setValue(0)
        self._speed_label.clear()
        self._stage_label.setText("Starting...")
        self._cancel_requested = False

        cutter = SmartCutter(job, self)
        cutter.stage_changed.connect(self._on_stage)
        cutter.progress.connect(self._on_progress)
        cutter.succeeded.connect(self._on_succeeded)
        cutter.failed.connect(self._on_failed)
        cutter.cancelled.connect(self._on_cancelled)
        cutter.finished.connect(self._on_thread_finished)  # emitted after all of the above
        self._cutter = cutter

        self._update_controls()
        cutter.start()

    @Slot()
    def _cancel_cut(self) -> None:
        if self._cutter is None:
            return
        self._cancel_requested = True
        self._cutter.request_cancel()
        self._stage_label.setText("Cancelling...")
        self._update_controls()

    @Slot(str, int, int)
    def _on_stage(self, label: str, index: int, count: int) -> None:
        if self._cancel_requested:
            return
        if count <= 0:  # preparation stage (e.g. converting MPEG-TS), not a numbered step
            self._stage_label.setText(label)
        else:
            self._stage_label.setText(f"Step {index} of {count}: {label}")
        self._progress.setValue(0)
        self._speed_label.clear()

    @Slot(float, float, float)
    def _on_progress(self, fraction: float, speed: float, eta_seconds: float) -> None:
        self._progress.setValue(int(math.floor(fraction * 1000.0 + 0.5)))
        parts = []
        if speed > 0:
            parts.append(f"Speed: {speed:.2f}x" if speed < 10 else f"Speed: {speed:.0f}x")
        if eta_seconds >= 0:
            parts.append(f"ETA: {format_eta(eta_seconds)}")
        self._speed_label.setText("    ".join(parts))

    @Slot(str)
    def _on_succeeded(self, summary: str) -> None:
        self._progress.setValue(1000)
        self._stage_label.setText("Done")
        self._speed_label.clear()
        QMessageBox.information(self, "Done", summary)

    @Slot(str)
    def _on_failed(self, message: str) -> None:
        self._stage_label.setText("Failed")
        self._speed_label.clear()
        QMessageBox.critical(self, "Error", message)

    @Slot()
    def _on_cancelled(self) -> None:
        self._progress.setValue(0)
        self._stage_label.setText("Cancelled")
        self._speed_label.clear()

    @Slot()
    def _on_thread_finished(self) -> None:
        cutter, self._cutter = self._cutter, None
        self._cancel_requested = False
        if cutter is not None:
            cutter.deleteLater()
        self._update_controls()

    # -- closing --------------------------------------------------------------------------------

    def closeEvent(self, event: QCloseEvent) -> None:  # noqa: N802
        cutter = self._cutter
        if cutter is not None and cutter.isRunning():
            answer = QMessageBox.question(
                self, "Exit", "A cut is still running. Cancel it and exit?",
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
                QMessageBox.StandardButton.No)
            if answer != QMessageBox.StandardButton.Yes:
                event.ignore()
                return
            cutter.request_cancel()
            cutter.wait()  # returns once ffmpeg was killed and the worker finished
        self._probe.abort()
        self._store.set_window_geometry(self.saveGeometry())
        event.accept()
