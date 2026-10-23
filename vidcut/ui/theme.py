"""Application-wide dark theme (Fusion style + explicit palette, identical on every OS)."""
from __future__ import annotations

from PySide6.QtCore import Qt
from PySide6.QtGui import QColor, QPalette
from PySide6.QtWidgets import QApplication

# Colour tokens - the only place where theme colours are defined.
WINDOW = QColor(32, 33, 36)
BASE = QColor(24, 25, 27)  # text fields, list backgrounds
ALT_BASE = QColor(38, 40, 43)
BUTTON = QColor(50, 52, 56)
TEXT = QColor(230, 230, 230)
DISABLED_TEXT = QColor(145, 147, 151)
PLACEHOLDER = QColor(140, 142, 146)
ACCENT = QColor(38, 110, 200)  # white text on it >= 4.5:1
ACCENT_TEXT = QColor(255, 255, 255)
LINK = QColor(110, 175, 255)
LINK_VISITED = QColor(185, 150, 240)
TOOLTIP_BASE = QColor(58, 60, 65)
BRIGHT_TEXT = QColor(255, 90, 90)


def build_dark_palette() -> QPalette:
    """Builds the dark palette (pure function, no QApplication needed)."""
    R, G = QPalette.ColorRole, QPalette.ColorGroup
    p = QPalette()
    p.setColor(R.Window, WINDOW)
    p.setColor(R.WindowText, TEXT)
    p.setColor(R.Base, BASE)
    p.setColor(R.AlternateBase, ALT_BASE)
    p.setColor(R.ToolTipBase, TOOLTIP_BASE)
    p.setColor(R.ToolTipText, TEXT)
    p.setColor(R.Text, TEXT)
    p.setColor(R.PlaceholderText, PLACEHOLDER)
    p.setColor(R.Button, BUTTON)
    p.setColor(R.ButtonText, TEXT)
    p.setColor(R.BrightText, BRIGHT_TEXT)
    p.setColor(R.Link, LINK)
    p.setColor(R.LinkVisited, LINK_VISITED)
    p.setColor(R.Highlight, ACCENT)
    p.setColor(R.HighlightedText, ACCENT_TEXT)
    # Bevel / frame shades used by the Fusion style (must be set, or borders look light).
    p.setColor(R.Light, QColor(72, 75, 80))
    p.setColor(R.Midlight, QColor(60, 63, 67))
    p.setColor(R.Mid, QColor(44, 46, 50))
    p.setColor(R.Dark, QColor(18, 19, 21))
    p.setColor(R.Shadow, QColor(0, 0, 0))

    for role in (R.WindowText, R.Text, R.ButtonText):
        p.setColor(G.Disabled, role, DISABLED_TEXT)
    p.setColor(G.Disabled, R.Highlight, QColor(70, 72, 76))
    p.setColor(G.Disabled, R.HighlightedText, DISABLED_TEXT)
    p.setColor(G.Disabled, R.Base, QColor(30, 31, 34))
    return p


def apply_dark_theme(app: QApplication) -> None:
    """Dark look on every OS, regardless of the system light/dark setting."""
    hints = app.styleHints()
    if hasattr(hints, "setColorScheme"):  # Qt >= 6.8
        hints.setColorScheme(Qt.ColorScheme.Dark)
    app.setStyle("Fusion")
    app.setPalette(build_dark_palette())
