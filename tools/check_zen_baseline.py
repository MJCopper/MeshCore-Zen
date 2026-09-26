#!/usr/bin/env python3
"""Fail when Zen changes files owned by the MeshCore baseline."""

from pathlib import Path
import hashlib
import json
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
BASELINE = "d9296435"
PROTECTED = (
    "src",
    "include",
    "create-uf2.py",
    "variants/wio-tracker-l1",
    "variants/wio-tracker-l1-eink",
    "examples/companion_radio",
)

OVERLAY = ROOT / "examples/companion_radio/zen-overlay"
SHADOW_MANIFEST = OVERLAY / "build/shadow_manifest.json"
SHADOW_ROOTS = (("app", ROOT / "examples/companion_radio"),
                ("src", ROOT / "src"),
                ("variants/wio-tracker-l1", ROOT / "variants/wio-tracker-l1"))
FORBIDDEN_SHADOWS = {
    "src/Dispatcher.cpp", "src/Dispatcher.h", "src/Mesh.cpp", "src/Mesh.h",
    "src/helpers/BaseChatMesh.cpp", "src/helpers/BaseChatMesh.h",
    "src/helpers/ArduinoSerialInterface.cpp",
    "src/helpers/ArduinoSerialInterface.h",
    "src/helpers/BaseSerialInterface.h",
    "src/helpers/nrf52/SerialBLEInterface.cpp",
    "src/helpers/nrf52/SerialBLEInterface.h",
    "src/helpers/radiolib/RadioLibWrappers.cpp",
    "src/helpers/radiolib/RadioLibWrappers.h",
}

FORBIDDEN_APP_SHADOWS = {
    "DataStore.cpp", "DataStore.h", "NodePrefs.h",
}


def check_shadows() -> list[str]:
    manifest = json.loads(SHADOW_MANIFEST.read_text())
    errors = []
    actual = set()
    for namespace, baseline_root in SHADOW_ROOTS:
        overlay_root = OVERLAY / namespace
        for path in overlay_root.rglob("*"):
            if not path.is_file():
                continue
            relative = path.relative_to(overlay_root)
            baseline = baseline_root / relative
            if not baseline.exists():
                continue
            key = f"{namespace}/{relative.as_posix()}"
            actual.add(key)
            if key in FORBIDDEN_SHADOWS:
                errors.append(f"forbidden core shadow: {key}")
            entry = manifest.get(key)
            if not entry:
                errors.append(f"unreviewed baseline shadow: {key}")
                continue
            digest = hashlib.sha256(baseline.read_bytes()).hexdigest()
            if digest != entry.get("baseline_sha256"):
                errors.append(f"upstream counterpart changed: {key}")
            overlay_digest = hashlib.sha256(path.read_bytes()).hexdigest()
            if overlay_digest != entry.get("overlay_sha256"):
                errors.append(f"unreviewed overlay shadow change: {key}")
    for key in manifest.keys() - actual:
        errors.append(f"stale shadow manifest entry: {key}")
    app_root = OVERLAY / "app"
    for name in FORBIDDEN_APP_SHADOWS:
        if app_root.joinpath(name).exists():
            errors.append(f"forbidden baseline storage shadow: app/{name}")

    build_config = (ROOT / "variants/zen-wio-tracker-l1/platformio.ini").read_text()
    if "+<../examples/companion_radio/DataStore.cpp>" not in build_config:
        errors.append("Zen build does not compile baseline DataStore.cpp directly")
    return errors


def main() -> int:
    command = ["git", "diff", "--name-only", BASELINE, "--", *PROTECTED]
    result = subprocess.run(command, cwd=ROOT, check=False, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if result.returncode:
        print(result.stderr.strip(), file=sys.stderr)
        return result.returncode
    changed = [line for line in result.stdout.splitlines()
               if line and not line.startswith("examples/companion_radio/zen-overlay/")]
    shadow_errors = check_shadows()
    if changed:
        print("Zen baseline boundary violation:", file=sys.stderr)
        for path in changed:
            print(f"  {path}", file=sys.stderr)
        print("Move Zen behaviour into examples/companion_radio/zen-overlay.",
              file=sys.stderr)
        return 1
    if shadow_errors:
        print("Zen shadow boundary violation:", file=sys.stderr)
        for error in shadow_errors:
            print(f"  {error}", file=sys.stderr)
        return 1
    print(f"MeshCore baseline {BASELINE} is unchanged in protected paths.")
    print("Zen baseline shadows match the reviewed manifest.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
