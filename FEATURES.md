# BME680 sensor features

This firmware specialises MeshCore v1.17.1 for a fixed environmental sensor
built from a XIAO nRF52840, an SX1262 radio and a BME680.

| Area | Behaviour |
| ---- | --------- |
| Hardware | Seeed Studio XIAO nRF52840 with an SX1262 radio and BME680 at I2C address `0x76` |
| I2C | BME680 SCL on `D6` and SDA on `D7`; the radio uses the standard `D4`/`D5` positions |
| Sampling | Temperature, humidity, pressure, altitude and air quality sampled once per minute |
| Telemetry | Current BME680 readings and node supply voltage returned through the MeshCore sensor protocol |
| Altitude | Pressure-derived altitude with a fixed `+100 m` installation offset |
| Air quality | Relative `0–500` score, ten-sample warm-up and Good through Hazardous labels |
| Public bot | `!hillvue` command on the built-in Public channel with selectable measurements, ping, incoming path and round-trip traces of up to ten repeaters |
| Traffic control | Four accepted bot commands per rolling minute; replies may use two numbered messages three seconds apart, with an eight-command duplicate cache and no automatic channel posts |
| Mesh role | Sensor leaf node; packet forwarding is fixed Off |
| Storage | Identity, configuration, ACL and a bounded repeater-name cache persist; readings, air baseline and bot rate-limit state remain in RAM |
| Management | MeshCore sensor telemetry, administrator login, node configuration and USB serial console |
| Radio | SX1262 support with persisted MeshCore radio settings and a maximum configured TX power of 22 dBm |

Detailed guides are linked from [README.md](./README.md).
