# Building the Public Archive

[Back to README](https://github.com/MJCopper/MeshCore-Zen/blob/meshcore/archiver/README.md)

The firmware is built with Python 3 and
[PlatformIO Core](https://platformio.org/install/cli). Install Git, Python 3 and
PlatformIO, then clone the repository and select the archive branch:

```sh
git clone https://github.com/MJCopper/MeshCore-Zen.git
cd MeshCore-Zen
git switch meshcore/archiver
python3 -m pip install --upgrade platformio
```

PlatformIO downloads the nRF52 framework, board platform and libraries during
the first build, so that build requires internet access and takes longer.

## Firmware

Run these commands from the repository root:

```sh
pio run -e Xiao_nrf52_archiver
pio run -e Xiao_nrf52_archiver -t create_uf2
```

The first command creates the ELF, HEX and Nordic DFU ZIP files. The second
converts the compiled HEX into a UF2 for the XIAO USB bootloader. The main
artifacts are:

```text
.pio/build/Xiao_nrf52_archiver/firmware.uf2
.pio/build/Xiao_nrf52_archiver/firmware.hex
.pio/build/Xiao_nrf52_archiver/firmware.zip
```

Flash `firmware.uf2` using the USB bootloader procedure in the
[README](https://github.com/MJCopper/MeshCore-Zen/blob/meshcore/archiver/README.md#flashing). The ZIP is intended for a compatible Nordic DFU
workflow.

To rebuild from scratch:

```sh
pio run -e Xiao_nrf52_archiver -t clean
pio run -e Xiao_nrf52_archiver
pio run -e Xiao_nrf52_archiver -t create_uf2
```

## Target configuration

The target is defined in `variants/xiao_nrf52/platformio.ini` with these custom
build settings:

- `PUBLIC_CHANNEL_ARCHIVE=1`
- `MAX_UNSYNCED_POSTS=256`
- `MAX_CLIENTS=12`
- `ADVERT_NAME="Public Archive"`
- `ADMIN_PASSWORD="password"`

Existing settings stored on the device can override the name, password and
radio defaults.
