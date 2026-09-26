#!/usr/bin/env python3
"""Show what a reviewed Zen shadow file changes versus its MeshCore baseline
counterpart, so upstream drift is visible on review instead of only a
pass/fail hash check."""

from pathlib import Path
import argparse
import difflib
import json
import sys

from check_zen_baseline import OVERLAY, SHADOW_MANIFEST, SHADOW_ROOTS


def baseline_path_for(key: str) -> Path:
    namespace, _, relative = key.partition("/")
    for root_namespace, baseline_root in SHADOW_ROOTS:
        if root_namespace == namespace:
            return baseline_root / relative
    raise KeyError(f"unknown shadow namespace: {namespace}")


def overlay_path_for(key: str) -> Path:
    namespace, _, relative = key.partition("/")
    return OVERLAY / namespace / relative


def diff_shadow(key: str) -> str:
    baseline = baseline_path_for(key)
    overlay = overlay_path_for(key)
    baseline_lines = baseline.read_text(errors="replace").splitlines(keepends=True) \
        if baseline.exists() else []
    overlay_lines = overlay.read_text(errors="replace").splitlines(keepends=True) \
        if overlay.exists() else []
    return "".join(difflib.unified_diff(
        baseline_lines, overlay_lines,
        fromfile=f"baseline/{key}", tofile=f"overlay/{key}"))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("shadow", nargs="?",
                        help="Manifest key to diff, e.g. app/MyMesh.cpp. "
                             "Diffs every reviewed shadow when omitted.")
    args = parser.parse_args()

    manifest = json.loads(SHADOW_MANIFEST.read_text())
    keys = [args.shadow] if args.shadow else sorted(manifest)
    unknown = [key for key in keys if key not in manifest]
    if unknown:
        print(f"Not in shadow manifest: {', '.join(unknown)}", file=sys.stderr)
        return 1

    for key in keys:
        diff = diff_shadow(key)
        print(f"=== {key} ===")
        print(diff if diff else "(overlay matches baseline exactly)")
        print()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
