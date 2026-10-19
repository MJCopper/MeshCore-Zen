# Building the BME680 Sensor

[Back to README](https://github.com/MJCopper/MeshCore-Zen/blob/meshcore/sensor/README.md)

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
[README](https://github.com/MJCopper/MeshCore-Zen/blob/meshcore/sensor/README.md#flashing).
Use `Xiao_nrf52_bme680_sensor.zip` with a compatible nRF52 DFU application for
OTA updates.

To rebuild from scratch:

```sh
pio run -e Xiao_nrf52_bme680_sensor -t clean
pio run -e Xiao_nrf52_bme680_sensor
pio run -e Xiao_nrf52_bme680_sensor -t create_uf2
```

## Target configuration

The target is defined in `variants/xiao_nrf52/platformio.ini`. It selects the
application in `examples/bme680_sensor/`, the BME680 sensor-manager adapter and
a 60-second sample interval. Other environmental drivers are disabled for this
target.

Installation-specific values are also defined there:

- `TELEM_BME680_ALTITUDE_OFFSET_M=100.0f`
- `ADVERT_NAME="BME680 Sensor"`
- `ADMIN_PASSWORD="password"`
- `SENSOR_BOT_CHANNEL_NAME="#hillvue"` (additional watched channel)

Public is always watched. Remove `SENSOR_BOT_CHANNEL_NAME` to watch only Public.
Requests and replies always use the same channel, including delayed responses.

Change defaults before building when the firmware will be installed elsewhere.
Existing settings stored on the device can override build defaults.

## Regression tests

```sh
pio test -e native -e native_sensor -e native_kiss_modem
bash bin/check-sensor-boundaries.sh
pio run -e Xiao_nrf52_sensor_baseline
```

The baseline target compiles the standard sensor application with the raw
BME680 driver. It checks that custom access and air-quality policy does not
leak into the upstream application.

See [Development and Testing](./sensor_features/development/development.md)
for coverage and the hardware acceptance procedure.

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
