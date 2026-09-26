# Building Zen

[Back to README](../README.md)

Zen is built with Python 3 and [PlatformIO Core](https://platformio.org/install/cli).
Install Git, Python 3 and PlatformIO, then clone the repository:

```sh
git clone https://github.com/MJCopper/MeshCore-Zen.git
cd MeshCore-Zen
python3 -m pip install --upgrade platformio
```

Run commands from the repository root. PlatformIO downloads the board platform,
framework and libraries on the first build, so that build needs internet access
and takes longer.

## Firmware

Build both supported display targets:

```sh
pio run -e WioTrackerL1_Zen_OLED
pio run -e WioTrackerL1_Zen_E-INK
```

The versioned UF2 files are written directly to:

```text
.pio/build/WioTrackerL1_Zen_OLED/WioTrackerL1_Zen_OLED.<version>.uf2
.pio/build/WioTrackerL1_Zen_E-INK/WioTrackerL1_Zen_E-INK.<version>.uf2
```

The version is taken from `FIRMWARE_VERSION` in
`examples/companion_radio/MyMesh.h`, without its leading `v`. PlatformIO also
creates `firmware.zip` in each target directory for BLE DFU.

To rebuild a target from scratch:

```sh
pio run -e WioTrackerL1_Zen_OLED -t clean
pio run -e WioTrackerL1_Zen_E-INK -t clean
pio run -e WioTrackerL1_Zen_OLED
pio run -e WioTrackerL1_Zen_E-INK
```

Flash a locally built UF2 with the same Reset-button and USB-drive procedure in
the [README](../README.md#flashing). Always export a fresh settings backup with
the MeshCore companion app before flashing, including development builds.
Verify that the target name matches the physical display before copying it.

For release-style filenames, build both targets through the repository script:

```sh
bash build.sh build-zen-firmwares
```

Release files are written to `out/` using the same UF2 names; BLE-DFU packages
use `<target>.<version>.ota.zip`. The script recreates `out/` at the start of
every invocation, so copy any files you want to retain before running it again.

## Tests

Run the host-side test suite before building firmware:

```sh
pio test -e native
```

GitHub Actions runs the same tests and builds both Zen display targets. Release
tags beginning with `v` create a draft release containing UF2 and BLE-DFU files.

## Documentation

Install the documentation dependencies and preview the site locally:

```sh
python3 -m pip install mkdocs mkdocs-material
mkdocs serve
```

Validate the complete site before publishing:

```sh
mkdocs build --strict
```

## Common build problems

- If `pio` is not found, activate the Python environment where PlatformIO was
  installed or run it as `python3 -m platformio`.
- If dependency installation fails, confirm internet access and rerun the same
  command; PlatformIO reuses completed downloads.
- If a build behaves inconsistently after switching branches, clean both target
  environments and rebuild.
- If no UF2 appears, check the final PlatformIO summary for `SUCCESS` and use the
  exact versioned path shown above.
