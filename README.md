# MeshCore BME680 Sensor Firmware

> **First installation:** Read the [flashing notes](#flashing) before connecting
> the sensor node to the mesh.

This branch provides dedicated environmental-sensor firmware for a Seeed Studio
XIAO nRF52840, an SX1262 LoRa radio and a Bosch BME680. It is based on
**MeshCore v1.17.1** and uses the standard MeshCore sensor protocol.

## Supported hardware

| Component | Requirement |
| --------- | ----------- |
| Controller | Seeed Studio XIAO nRF52840 |
| Radio | SX1262 module wired for the XIAO target |
| Sensor | BME680 I2C breakout at address `0x76` |
| Display | None |
| Power | USB or a suitable 3.3 V/battery supply |

The BME680 uses `D6` for SCL and `D7` for SDA. These are firmware-specific I2C
pins because the radio occupies the XIAO's usual `D4` and `D5` connections.
See [Hardware](./docs/sensor_features/hardware/hardware.md) for the complete
wiring table.

## Software features

- Temperature, relative humidity, pressure, calculated altitude and relative
  air-quality telemetry.
- One sensor sample per minute with the latest result cached in RAM.
- Public-channel queries for environmental readings, battery voltage, reachability,
  repeater path and round-trip traces of up to ten repeaters through the
  case-insensitive `!hillvue` command.
- Single- and multi-value command responses with consistent labels and units;
  longer replies use at most two numbered messages, scheduled three seconds apart.
- Ten-sample air-quality warm-up with descriptive quality ratings.
- RAM-only air-quality calibration and duplicate-command tracking.
- Standard MeshCore remote telemetry and administrator management.
- Fixed leaf-node operation: received mesh packets are never repeated.
- Up to four accepted bot commands per rolling minute, with no scheduled sensor
  posts.

See [FEATURES.md](./FEATURES.md) for the complete behaviour summary.

> [!WARNING]
> This is custom sensor firmware. Confirm the wiring, radio frequency and legal
> transmit settings for your hardware and location before powering the node.

## Flashing

Flashing replaces the installed firmware. Erasing first also removes the saved
node identity, radio settings, ACL and administrator password.

1. Build or obtain `Xiao_nrf52_bme680_sensor.uf2`.
2. Connect the XIAO nRF52840 directly to the computer with a USB data cable. A
   charge-only cable cannot transfer firmware.
3. Quickly press the XIAO's **Reset** button twice. A removable bootloader drive
   should appear on the computer.
4. Copy `Xiao_nrf52_bme680_sensor.uf2` to the root of that drive.
5. Wait for the copy to finish. The drive normally disconnects and the XIAO
   restarts automatically when flashing completes.
6. If the drive does not appear, reconnect the USB cable and repeat the quick
   double-press of **Reset**.

For a clean first installation, use the nRF52 erase UF2 from the
[MeshCore Flasher](https://meshcore.io/flasher) before copying the sensor UF2.

## First setup

1. Connect the BME680 and SX1262 using the documented wiring.
2. Flash the sensor firmware and allow the node to boot.
3. Discover the node as a Sensor in a MeshCore companion app.
4. Log in with the configured administrator password; the build default is
   `password` and should be changed immediately.
5. Set the node name and the correct regional radio parameters.
6. Wait for the first one-minute sample before requesting telemetry. Air quality
   reports **Warming Up** until ten successful samples have completed.

The node advertises as `BME680 Sensor` on a clean configuration. Saved settings
from an earlier installation take precedence over build defaults.

## Guides

- [Getting Started](./docs/getting_started.md)
- [Hardware and Wiring](./docs/sensor_features/hardware/hardware.md)
- [Telemetry](./docs/sensor_features/telemetry/telemetry.md)
- [Public-channel Commands](./docs/sensor_features/public_bot/public_bot.md)
- [Access and Management](./docs/sensor_features/access/access.md)
- [Building the Firmware](./docs/building_sensor.md)

## Development

The sensor-specific code is isolated behind `PUBLIC_CHANNEL_SENSOR_BOT` and the
`Xiao_nrf52_bme680_sensor` PlatformIO environment. This keeps the custom
behaviour separate from the MeshCore v1.17.1 baseline.

Build the firmware with:

```sh
pio run -e Xiao_nrf52_bme680_sensor
pio run -e Xiao_nrf52_bme680_sensor -t create_uf2
```

The second command writes the flashable UF2 to:

```text
.pio/build/Xiao_nrf52_bme680_sensor/Xiao_nrf52_bme680_sensor.uf2
```

The first command also creates the OTA DFU package at
`.pio/build/Xiao_nrf52_bme680_sensor/Xiao_nrf52_bme680_sensor.zip`.

Mesh networking and the sensor protocol are provided by
[MeshCore](https://github.com/meshcore-dev/MeshCore).
