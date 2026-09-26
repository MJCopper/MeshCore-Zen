#!/usr/bin/python3

# Adds PlatformIO post-processing to convert hex files to uf2 files

import os
import re

Import("env")

firmware_hex = "${BUILD_DIR}/${PROGNAME}.hex"

def zen_version():
    header = os.path.join(env.subst("$PROJECT_DIR"), "examples", "companion_radio",
                          "zen-overlay", "app", "MyMesh.h")
    with open(header, "r", encoding="utf-8") as source:
        match = re.search(r'#define\s+FIRMWARE_VERSION\s+"v?([0-9]+\.[0-9]+\.[0-9]+)"',
                          source.read())
    if not match:
        raise RuntimeError("Unable to read the Zen version from MyMesh.h")
    return match.group(1)

uf2_basename = env.GetProjectOption("custom_uf2_basename", "")
if uf2_basename:
    uf2_file = os.path.join(env.subst("$BUILD_DIR"),
                            "%s.%s.uf2" % (uf2_basename, zen_version()))
else:
    uf2_file = os.environ.get("UF2_FILE_PATH", "${BUILD_DIR}/${PROGNAME}.uf2")

def create_uf2_action(source, target, env):
    uf2_cmd = " ".join(
        [
            '"$PYTHONEXE"',
            '"$PROJECT_DIR/bin/uf2conv/uf2conv.py"',
            '-f', '0xADA52840',
            '-c', firmware_hex,
            '-o', uf2_file,
        ]
    )
    env.Execute(uf2_cmd)

env.AddPostAction(firmware_hex, create_uf2_action)

env.AddCustomTarget(
    name="create_uf2",
    dependencies=firmware_hex,
    actions=create_uf2_action,
    title="Create UF2 file",
    description="Use uf2conv to convert hex binary into uf2",
    always_build=True,
)
