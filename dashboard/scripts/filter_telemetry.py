#!/usr/bin/env python3
"""Extract module four telemetry frames from a noisy xv6 console stream."""

from __future__ import annotations

import re
import sys

ANSI_RE = re.compile(r"\x1b\[[0-9;?]*[A-Za-z]")
KEYWORD_RE = re.compile(r"(DASHBOARDD_BEGIN|DASHBOARDD_END|DASHBOARDD_ERROR|SNAP\s+|PROC\s+|VMA\s+)")


def sanitize(line: str) -> str:
    clean = ANSI_RE.sub("", line)
    clean = clean.replace("\r", "")
    clean = clean.replace("$", "")
    return "".join(ch for ch in clean if ch == "\n" or ch == "\t" or 32 <= ord(ch) <= 126)


def main() -> int:
    for raw in sys.stdin:
        clean = sanitize(raw)
        match = KEYWORD_RE.search(clean)
        if not match:
            continue
        frame = clean[match.start():].strip()
        if not frame:
            continue
        print(frame, flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
