from __future__ import annotations

import os
import shutil
from typing import Optional


def replace_file(src: str, dst: str) -> Optional[str]:
    """Moves `src` over `dst` (replacing it). Falls back to copy+delete if a plain
    rename is impossible. Returns None on success, otherwise an error message."""
    try:
        os.replace(src, dst)  # atomically replaces dst
        return None
    except OSError:
        pass

    try:
        shutil.copyfile(src, dst)
    except OSError as e:
        return f"Cannot write output file (is it open in another program?): {e.strerror or e}"
    try:
        os.remove(src)
    except OSError:
        pass
    return None
