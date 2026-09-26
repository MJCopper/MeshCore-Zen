#!/usr/bin/env python3
"""Print the canonical Zen firmware version from the overlay header."""

from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "examples/companion_radio/zen-overlay/app/MyMesh.h"


def main() -> int:
    match = re.search(
        r'^#define\s+FIRMWARE_VERSION\s+"(v[0-9]+\.[0-9]+\.[0-9]+)"',
        HEADER.read_text(encoding="utf-8"), re.MULTILINE)
    if not match:
        print(f"Unable to read Zen version from {HEADER}", file=sys.stderr)
        return 1
    version = match.group(1)
    if "--bare" in sys.argv:
        version = version.removeprefix("v")
    print(version)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
