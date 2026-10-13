from __future__ import annotations

import math
import re
from typing import Optional

# [+-]digits[.digits][e[+-]digits]  (ASCII digits only; no "nan", "inf", "1_000", ...)
_NUMBER = re.compile(r"[+-]?(?:[0-9]+\.?[0-9]*|\.[0-9]+)(?:[eE][+-]?[0-9]+)?")


def parse_double(text: str) -> Optional[float]:
    """Strict, locale-independent string -> float.

    Returns None for anything that is not a plain decimal number
    (e.g. "N/A", "", "12abc", "nan", "inf").
    """
    s = text.strip()
    if not _NUMBER.fullmatch(s):
        return None
    try:
        value = float(s)
    except ValueError:
        return None
    return value if math.isfinite(value) else None
