#!/usr/bin/env python3
"""Fail when Zen changes files owned by the MeshCore baseline."""

from pathlib import Path
import hashlib
import json
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[1]
BASELINE = "81dbae592ffad40b4212bc0b37e82b38da08f4bc"
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


def reviewed_dispatcher_patch(name: str, expected: bytes, entry: dict) -> bytes:
    """Apply the sole authorised multi-flood patch to the immutable trace pin."""
    patch_name = entry.get("extension_patch")
    if patch_name != "tools/zen_multi_flood_dispatcher.patch":
        raise ValueError("only the multi-flood Dispatcher patch is permitted")
    data = (ROOT / patch_name).read_bytes()
    if hashlib.sha256(data).hexdigest() != entry.get("extension_patch_sha256"):
        raise ValueError("multi-flood patch digest mismatch")
    paths = {line[6:] for line in data.decode().splitlines() if line.startswith("+++ b/")}
    if paths != {"src/Dispatcher.h", "src/Dispatcher.cpp"}:
        raise ValueError("multi-flood patch must touch only the two Dispatcher files")
    if entry["source_commit"] != "99dc14489bcaf529be7957a998863c5acc8713e8":
        raise ValueError("multi-flood must preserve the immutable sensor trace base")
    with tempfile.TemporaryDirectory() as directory:
        target = Path(directory) / name
        target.parent.mkdir(parents=True)
        target.write_bytes(expected)
        applied = subprocess.run(["git", "apply", "--unsafe-paths", "--include=" + name, "-"],
                                 input=data, cwd=directory, capture_output=True, check=False)
        if applied.returncode:
            raise ValueError("multi-flood patch no longer applies to its trace base")
        return target.read_bytes()


def checked_core_extensions() -> tuple[set[str], list[str]]:
    """Pinned trace hooks, the sole multi-flood exception and the exact RTC hook."""
    manifest = json.loads((ROOT / "tools/zen_baseline_extensions.json").read_text())
    errors = []
    allowed = {"src/Dispatcher.h", "src/Dispatcher.cpp", "src/MeshCore.h"}
    if set(manifest) != allowed:
        return set(), ["core extension manifest must contain exactly Dispatcher and the RTC hook"]
    for name, entry in manifest.items():
        source = subprocess.run(["git", "show", f"{entry['source_commit']}:{name}"],
                                cwd=ROOT, capture_output=True, check=False)
        expected = source.stdout
        if name == "src/MeshCore.h":
            before = b"  uint32_t getCurrentTimeUnique() {"
            after = b"  virtual uint32_t getCurrentTimeUnique() {"
            if entry['source_commit'] != BASELINE or expected.count(before) != 1:
                errors.append("RTC extension must be the single virtual hook on the pinned baseline")
            expected = expected.replace(before, after, 1)
        if name.startswith("src/Dispatcher") and "extension_patch" in entry:
            try:
                expected = reviewed_dispatcher_patch(name, expected, entry)
            except (OSError, ValueError) as error:
                errors.append(f"invalid multi-flood extension: {name}: {error}")
        digest = hashlib.sha256(expected).hexdigest()
        if source.returncode or digest != entry["sha256"] or expected != (ROOT/name).read_bytes():
            errors.append(f"unreviewed core extension: {name}")
    return allowed, errors


def main() -> int:
    command = ["git", "diff", "--name-only", BASELINE, "--", *PROTECTED]
    result = subprocess.run(command, cwd=ROOT, check=False, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if result.returncode:
        print(result.stderr.strip(), file=sys.stderr)
        return result.returncode
    changed = [line for line in result.stdout.splitlines()
               if line and not line.startswith("examples/companion_radio/zen-overlay/")]
    extensions, extension_errors = checked_core_extensions()
    changed = [name for name in changed if name not in extensions]
    shadow_errors = check_shadows() + extension_errors
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
    print(f"MeshCore baseline {BASELINE} is unchanged outside reviewed trace/RTC hooks and the sole multi-flood exception.")
    print("Zen baseline shadows match the reviewed manifest.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
