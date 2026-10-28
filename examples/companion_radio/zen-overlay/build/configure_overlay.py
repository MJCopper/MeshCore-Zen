"""Expose Zen-owned extensions before the unmodified MeshCore source tree."""

import os
import subprocess
import sys

Import("env")

project = env.subst("$PROJECT_DIR")
subprocess.run([sys.executable, os.path.join(project, "tools", "zen_radio_presets.py"), "--check"], check=True)
env.Prepend(CPPPATH=[
    os.path.join(project, "examples", "companion_radio", "zen-overlay", "src"),
    os.path.join(project, "examples", "companion_radio", "zen-overlay",
                 "variants", "wio-tracker-l1"),
    os.path.join(project, "examples", "companion_radio", "zen-overlay", "app"),
    os.path.join(project, "examples", "companion_radio", "zen-overlay", "app",
                 "ui-new"),
])

# Zen extensions inherit unchanged MeshCore interfaces and reuse baseline
# sibling headers. Make those namespaces explicit without global forwarding
# headers or compiler-specific include_next routing.
env.AppendUnique(CPPPATH=[
    os.path.join(project, "src"),
    os.path.join(project, "src", "helpers"),
    os.path.join(project, "src", "helpers", "nrf52"),
    os.path.join(project, "src", "helpers", "radiolib"),
    os.path.join(project, "src", "helpers", "sensors"),
    os.path.join(project, "src", "helpers", "ui"),
    os.path.join(project, "examples", "companion_radio"),
])
