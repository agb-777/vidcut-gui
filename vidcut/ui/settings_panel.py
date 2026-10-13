from __future__ import annotations

from typing import Optional

from PySide6.QtCore import Qt
from PySide6.QtWidgets import (
    QComboBox, QFormLayout, QGroupBox, QHBoxLayout, QPushButton, QSlider, QSpinBox,
    QVBoxLayout, QWidget,
)

from ..core.encode_settings import (
    AUDIO_BITRATE_CHOICES, CRF_MAX, CRF_MIN, PRESETS, EncodeSettings, default_settings,
)


class SettingsPanel(QGroupBox):
    """The "Encoding Settings" box: preset, CRF and audio bitrate chosen by the user."""

    def __init__(self, parent: Optional[QWidget] = None) -> None:
        super().__init__("Encoding Settings", parent)

        self._preset = QComboBox(self)
        self._preset.addItems(PRESETS)
        self._preset.setToolTip("x264/x265 speed preset. Slower = smaller file / better quality.")

        self._crf_slider = QSlider(Qt.Orientation.Horizontal, self)
        self._crf_slider.setRange(CRF_MIN, CRF_MAX)
        self._crf_spin = QSpinBox(self)
        self._crf_spin.setRange(CRF_MIN, CRF_MAX)
        crf_tip = "Constant Rate Factor. Lower = higher quality and larger file."
        self._crf_slider.setToolTip(crf_tip)
        self._crf_spin.setToolTip(crf_tip)
        self._crf_slider.valueChanged.connect(self._crf_spin.setValue)
        self._crf_spin.valueChanged.connect(self._crf_slider.setValue)

        self._bitrate = QComboBox(self)
        self._bitrate.setEditable(True)
        self._bitrate.setInsertPolicy(QComboBox.InsertPolicy.NoInsert)
        self._bitrate.addItems(AUDIO_BITRATE_CHOICES)
        self._bitrate.setToolTip("AAC audio bitrate. Pick one or type your own, e.g. 224k or 1M.")

        crf_row = QHBoxLayout()
        crf_row.addWidget(self._crf_slider, 1)
        crf_row.addWidget(self._crf_spin)

        form = QFormLayout()
        form.addRow("Preset:", self._preset)
        form.addRow("CRF:", crf_row)
        form.addRow("Audio bitrate:", self._bitrate)

        reset = QPushButton("Reset to Defaults", self)
        reset.clicked.connect(self.reset_to_defaults)

        bottom = QHBoxLayout()
        bottom.addStretch(1)
        bottom.addWidget(reset)

        root = QVBoxLayout(self)
        root.addLayout(form)
        root.addLayout(bottom)

        self.set_settings(default_settings())

    def settings(self) -> EncodeSettings:
        return EncodeSettings(
            preset=self._preset.currentText(),
            crf=self._crf_spin.value(),
            audio_bitrate=self._bitrate.currentText().strip(),
        )

    def set_settings(self, s: EncodeSettings) -> None:
        idx = self._preset.findText(s.preset)
        self._preset.setCurrentIndex(idx if idx >= 0 else 0)
        self._crf_spin.setValue(s.crf)
        self._bitrate.setCurrentText(s.audio_bitrate)

    def reset_to_defaults(self) -> None:
        self.set_settings(default_settings())
