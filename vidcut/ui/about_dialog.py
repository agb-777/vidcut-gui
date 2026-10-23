from __future__ import annotations

import html
import platform
from typing import Optional

import PySide6
from PySide6.QtCore import Qt, qVersion
from PySide6.QtWidgets import QDialog, QDialogButtonBox, QLabel, QVBoxLayout, QWidget

from .. import appinfo


class AboutDialog(QDialog):
    def __init__(self, parent: Optional[QWidget] = None) -> None:
        super().__init__(parent)
        self.setWindowTitle(f"About {appinfo.NAME}")

        title = QLabel(f"<h2>{appinfo.NAME}</h2>", self)
        version = QLabel(f"Version {appinfo.VERSION}", self)
        runtime = QLabel(
            f"Python {platform.python_version()}, PySide6 {PySide6.__version__}, "
            f"Qt {qVersion()}", self)
        desc = QLabel(
            "Fast, lossless-where-possible video cutter.\n"
            "Uses FFmpeg: keyframe-aligned parts are copied without re-encoding, "
            "only the cut edges are re-encoded.", self)
        desc.setWordWrap(True)

        author = QLabel(f"Author: {html.escape(appinfo.AUTHOR)}", self)
        homepage = QLabel(
            f'Homepage: <a href="{html.escape(appinfo.HOMEPAGE, quote=True)}">'
            f"{html.escape(appinfo.HOMEPAGE)}</a>", self)
        homepage.setTextFormat(Qt.TextFormat.RichText)
        homepage.setOpenExternalLinks(True)  # opens in the default browser
        homepage.setTextInteractionFlags(
            Qt.TextInteractionFlag.TextBrowserInteraction)  # clickable + selectable

        buttons = QDialogButtonBox(QDialogButtonBox.StandardButton.Ok, self)
        buttons.accepted.connect(self.accept)

        layout = QVBoxLayout(self)
        layout.addWidget(title)
        layout.addWidget(version)
        layout.addWidget(runtime)
        layout.addSpacing(8)
        layout.addWidget(desc)
        layout.addSpacing(8)
        layout.addWidget(author)
        layout.addWidget(homepage)
        layout.addSpacing(8)
        layout.addWidget(buttons)
        self.setMinimumWidth(380)
