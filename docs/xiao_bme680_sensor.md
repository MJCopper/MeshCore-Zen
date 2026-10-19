# XIAO nRF52840 BME680 Sensor

[Back to README](https://github.com/MJCopper/MeshCore-Zen/blob/meshcore/sensor/README.md)

The `Xiao_nrf52_bme680_sensor` target creates a display-free MeshCore
environmental sensor using a XIAO nRF52840, SX1262 and BME680. It measures once
per minute, serves current telemetry on request and answers rate-limited
`!hillvue` queries on Public and `#hillvue`, replying on the requesting channel.

The firmware is a leaf node. It receives traffic addressed to the sensor and
listens for its Public-channel command, but it does not forward packets for
other nodes.

## Guides

- [Hardware and Wiring](./sensor_features/hardware/hardware.md)
- [Telemetry](./sensor_features/telemetry/telemetry.md)
- [Public-channel Commands](./sensor_features/public_bot/public_bot.md)
- [Access and Management](./sensor_features/access/access.md)
- [Building the Firmware](./building_sensor.md)
