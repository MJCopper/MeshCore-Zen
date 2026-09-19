# Building the BME680 Sensor

[Back to README](../README.md)

The firmware is built with Python 3 and
[PlatformIO Core](https://platformio.org/install/cli). Install Git, Python 3 and
PlatformIO, then clone the repository and select the sensor branch:

```sh
git clone https://github.com/MJCopper/MeshCore-Zen.git
cd MeshCore-Zen
git switch meshcore/sensor
python3 -m pip install --upgrade platformio
```

PlatformIO downloads the nRF52 framework, board platform and libraries during
the first build, so that build requires internet access and takes longer.

## Firmware

Run this command from the repository root:

```sh
pio run -e Xiao_nrf52_bme680_sensor
pio run -e Xiao_nrf52_bme680_sensor -t create_uf2
```

The first command compiles the firmware and creates the HEX and named DFU ZIP.
The second converts the compiled HEX into the flashable UF2. The main artifacts are:

```text
.pio/build/Xiao_nrf52_bme680_sensor/Xiao_nrf52_bme680_sensor.uf2
.pio/build/Xiao_nrf52_bme680_sensor/Xiao_nrf52_bme680_sensor.zip
.pio/build/Xiao_nrf52_bme680_sensor/firmware.hex
```

Flash `Xiao_nrf52_bme680_sensor.uf2` using the USB bootloader procedure in the
[README](../README.md#flashing).
Use `Xiao_nrf52_bme680_sensor.zip` with a compatible nRF52 DFU application for
OTA updates.

To rebuild from scratch:

```sh
pio run -e Xiao_nrf52_bme680_sensor -t clean
pio run -e Xiao_nrf52_bme680_sensor
pio run -e Xiao_nrf52_bme680_sensor -t create_uf2
```

## Target configuration

The target is defined in `variants/xiao_nrf52/platformio.ini`. Its specialised
build flags enable only the BME680 environmental driver, air-quality estimator,
Public-channel bot and 60-second sample interval.

Installation-specific values are also defined there:

- `TELEM_BME680_ALTITUDE_OFFSET_M=100.0f`
- `ADVERT_NAME="BME680 Sensor"`
- `ADMIN_PASSWORD="password"`

Change defaults before building when the firmware will be installed elsewhere.
Existing settings stored on the device can override build defaults.

## Documentation

Install the documentation dependencies and preview the sensor site locally:

```sh
python3 -m pip install mkdocs mkdocs-material
mkdocs serve
```

Validate all links and pages with:

```sh
mkdocs build --strict
```
