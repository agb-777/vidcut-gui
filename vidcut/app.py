from __future__ import annotations

import sys

from PySide6.QtCore import QCoreApplication
from PySide6.QtGui import QColor, QPalette
from PySide6.QtWidgets import QApplication

from . import appinfo


def apply_light_theme(app: QApplication) -> None:
    """Plain white, "normal desktop application" look on every OS (ignores dark mode)."""
    hints = app.styleHints()
    if hasattr(hints, "setColorScheme"):  # Qt >= 6.8
        from PySide6.QtCore import Qt
        hints.setColorScheme(Qt.ColorScheme.Light)
    app.setStyle("Fusion")

    white, black = QColor(255, 255, 255), QColor(0, 0, 0)
    grey, accent, dis = QColor(240, 240, 240), QColor(0, 120, 215), QColor(160, 160, 160)
    R, G = QPalette.ColorRole, QPalette.ColorGroup
    p = QPalette()
    p.setColor(R.Window, white)
    p.setColor(R.WindowText, black)
    p.setColor(R.Base, white)
    p.setColor(R.AlternateBase, QColor(245, 245, 245))
    p.setColor(R.ToolTipBase, QColor(255, 255, 220))
    p.setColor(R.ToolTipText, black)
    p.setColor(R.Text, black)
    p.setColor(R.PlaceholderText, QColor(150, 150, 150))
    p.setColor(R.Button, grey)
    p.setColor(R.ButtonText, black)
    p.setColor(R.BrightText, QColor(255, 0, 0))
    p.setColor(R.Link, accent)
    p.setColor(R.Highlight, accent)
    p.setColor(R.HighlightedText, white)
    p.setColor(G.Disabled, R.WindowText, dis)
    p.setColor(G.Disabled, R.Text, dis)
    p.setColor(G.Disabled, R.ButtonText, dis)
    app.setPalette(p)


def main() -> int:
    app = QApplication(sys.argv)
    QCoreApplication.setOrganizationName("VidCut")
    QCoreApplication.setApplicationName(appinfo.NAME)
    QCoreApplication.setApplicationVersion(appinfo.VERSION)

    apply_light_theme(app)

    from .ui.main_window import MainWindow  # after the app names are set (QSettings)

    window = MainWindow()
    window.show()
    return app.exec()
