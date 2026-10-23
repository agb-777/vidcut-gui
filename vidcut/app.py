from __future__ import annotations

import sys

from PySide6.QtCore import QCoreApplication
from PySide6.QtWidgets import QApplication

from . import appinfo
from .ui.theme import apply_dark_theme


def main() -> int:
    app = QApplication(sys.argv)
    QCoreApplication.setOrganizationName("VidCut")
    QCoreApplication.setApplicationName(appinfo.NAME)
    QCoreApplication.setApplicationVersion(appinfo.VERSION)

    apply_dark_theme(app)

    from .ui.main_window import MainWindow  # after the app names are set (QSettings)

    window = MainWindow()
    window.show()
    return app.exec()
