from __future__ import annotations

import threading


class CutError(Exception):
    """Recoverable, user-presentable error."""


class CutCancelled(Exception):
    """Raised inside the worker when the user pressed Cancel."""


class CancelToken:
    """Thread-safe cancel flag shared between the UI thread and the worker."""

    def __init__(self) -> None:
        self._event = threading.Event()

    def cancel(self) -> None:
        self._event.set()

    def is_cancelled(self) -> bool:
        return self._event.is_set()
