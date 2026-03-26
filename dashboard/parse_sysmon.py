#!/usr/bin/env python3
"""Compatibility wrapper for the module four telemetry parser."""

import sys
from pathlib import Path

if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from dashboard.bridge.parser import dump_payload


def main(argv):
    if len(argv) < 2:
        print("Usage: python3 dashboard/parse_sysmon.py <raw-log> [output-json]")
        return 1

    source = Path(argv[1])
    target = Path(argv[2]) if len(argv) > 2 else Path("dashboard/data/snapshots.json")
    dump_payload(source, target)
    print(f"Wrote {target}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
