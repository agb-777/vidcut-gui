from __future__ import annotations

from PySide6.QtCore import QByteArray, QSettings

from ..core.encode_settings import EncodeSettings, default_settings, validate_settings


class SettingsStore:
    """Persistent user settings (QSettings: registry on Windows, plist on macOS, ini on Linux).

    QCoreApplication organization/application names must be set before use.
    """

    def __init__(self) -> None:
        self._s = QSettings()

    # -- encoding ----------------------------------------------------------------

    def load_encode_settings(self) -> EncodeSettings:
        d = default_settings()
        s = EncodeSettings(
            preset=str(self._s.value("encode/preset", d.preset)),
            crf=d.crf,
            audio_bitrate=str(self._s.value("encode/audioBitrate", d.audio_bitrate)),
        )
        try:
            s.crf = int(str(self._s.value("encode/crf", d.crf)))
        except (TypeError, ValueError):
            return d  # corrupted / edited by hand
        if validate_settings(s):
            return default_settings()
        return s

    def save_encode_settings(self, s: EncodeSettings) -> None:
        self._s.setValue("encode/preset", s.preset)
        self._s.setValue("encode/crf", s.crf)
        self._s.setValue("encode/audioBitrate", s.audio_bitrate)

    # -- paths -----------------------------------------------------------------------

    def ffmpeg_dir(self) -> str:
        return str(self._s.value("paths/ffmpegDir", "") or "")

    def set_ffmpeg_dir(self, directory: str) -> None:
        self._s.setValue("paths/ffmpegDir", directory)

    def last_dir(self, key: str) -> str:
        return str(self._s.value("paths/" + key, "") or "")

    def set_last_dir(self, key: str, directory: str) -> None:
        self._s.setValue("paths/" + key, directory)

    # -- window ------------------------------------------------------------------------

    def window_geometry(self) -> QByteArray:
        g = self._s.value("ui/geometry")
        if isinstance(g, QByteArray):
            return g
        if isinstance(g, (bytes, bytearray)):
            return QByteArray(bytes(g))
        return QByteArray()

    def set_window_geometry(self, g: QByteArray) -> None:
        self._s.setValue("ui/geometry", g)
