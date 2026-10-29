#!/usr/bin/env python3
"""Build and validate UF2 and OTA ZIP outputs for both opt-in repeaters."""

import argparse
import binascii
import configparser
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
from zipfile import ZipFile


ROOT = Path(__file__).resolve().parents[2]
TARGETS = ("Xiao_nrf52_repeater_doubler", "RAK_4631_repeater_doubler")
BOARDS = ("seeed-xiao-afruitnrf52-nrf52840", "rak4631")


def verify_uf2(path):
    data = path.read_bytes()
    if not data or len(data) % 512:
        raise ValueError(f"Invalid UF2 length: {path}")
    count = len(data) // 512
    for index in range(count):
        block = data[index * 512:(index + 1) * 512]
        magic0, magic1, flags, address, size, number, total, family = struct.unpack_from("<8I", block)
        if (magic0, magic1, struct.unpack_from("<I", block, 508)[0]) != (0x0A324655, 0x9E5D5157, 0x0AB16F30):
            raise ValueError(f"Invalid UF2 magic: {path}, block {index}")
        if not flags & 0x2000 or family != 0xADA52840 or not 0 < size <= 476 or number != index or total != count:
            raise ValueError(f"Invalid nRF52840 UF2 block: {path}, block {index}")
    return {"path": str(path.relative_to(ROOT)), "bytes": len(data),
            "sha256": hashlib.sha256(data).hexdigest(), "blocks": count}


def verify_ota_zip(path, board, version):
    with ZipFile(path) as archive:
        if archive.testzip() is not None:
            raise ValueError(f"Corrupt OTA ZIP: {path}")
        manifest = json.loads(archive.read("manifest.json"))["manifest"]
        if set(manifest) != {"application", "dfu_version"} or manifest["dfu_version"] != 0.5:
            raise ValueError(f"Expected application-only Nordic DFU package: {path}")
        application = manifest["application"]
        binary = archive.read(application["bin_file"])
        init = application["init_packet_data"]
        expected_sd = int(json.loads((ROOT / "boards" / f"{board}.json").read_text())
                          ["build"]["softdevice"]["sd_fwid"], 0)
        crc = binascii.crc_hqx(binary, 0xFFFF)
        if (not binary or version.encode() not in binary or init["device_type"] != 0x52
                or init["softdevice_req"] != [expected_sd] or init["firmware_crc16"] != crc):
            raise ValueError(f"OTA firmware version, board requirement or CRC mismatch: {path}")
        expected_init = struct.pack("<HHIHHH", init["device_type"], init["device_revision"],
                                    init["application_version"], 1, expected_sd, crc)
        if archive.read(application["dat_file"]) != expected_init:
            raise ValueError(f"OTA init packet disagrees with manifest: {path}")
    data = path.read_bytes()
    return {"path": str(path.relative_to(ROOT)), "bytes": len(data),
            "sha256": hashlib.sha256(data).hexdigest(), "softdevice_req": [expected_sd]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pio", default=shutil.which("pio") or str(Path.home() / ".platformio/penv/bin/pio"))
    args = parser.parse_args()
    config = configparser.ConfigParser(interpolation=None)
    config.read(Path(__file__).with_name("platformio.ini"))
    version = config["doubler"]["version"]
    manifest = {"version": version, "commit": subprocess.check_output(
        ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
        "working_tree_dirty": bool(subprocess.check_output(
            ["git", "status", "--porcelain"], cwd=ROOT, text=True).strip()), "artifacts": []}
    for target, board in zip(TARGETS, BOARDS):
        path = ROOT / ".pio/build" / target / f"{target}.{version.removeprefix('v')}.uf2"
        env = dict(os.environ, UF2_FILE_PATH=str(path))
        # The default target includes PlatformIO's flash/RAM size checks.
        subprocess.run([args.pio, "run", "-e", target], cwd=ROOT, env=env, check=True)
        subprocess.run([args.pio, "run", "-e", target, "-t", "create_uf2"],
                       cwd=ROOT, env=env, check=True)
        if version.encode() not in (path.parent / "firmware.elf").read_bytes():
            raise ValueError(f"Expected firmware version {version} missing from {target}")
        manifest["artifacts"].append(verify_uf2(path))
        ota_path = path.with_suffix(".ota.zip")
        shutil.copyfile(path.parent / "firmware.zip", ota_path)
        manifest["artifacts"].append(verify_ota_zip(ota_path, board, version))
    output = ROOT / ".pio/build/doubler-manifest.json"
    output.write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"Verified UF2 and OTA ZIP for both targets; manifest: {output}")


if __name__ == "__main__":
    main()
