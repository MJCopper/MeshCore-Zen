# Telemetry

[Back to README](../../../README.md)

The node samples the BME680 once every 60 seconds and caches the most recent
successful reading. A telemetry request returns cached node and sensor data; it
does not trigger an additional BME680 measurement.

| Measurement | Unit or representation |
| ----------- | ---------------------- |
| Supply voltage | Volts |
| Temperature | Degrees Celsius |
| Relative humidity | Percent |
| Barometric pressure | hPa |
| Calculated altitude | Metres |
| Relative air quality | `0–500` generic sensor value |

The BME680 measurements occupy telemetry channel 2. Node supply voltage uses the
standard self-telemetry channel. The same cached battery-voltage reading is
available through `!hillvue voltage` on the Public channel.

## Altitude

Altitude is calculated from barometric pressure using a fixed sea-level
reference of `1013.25 hPa`, then adjusted by `+100 m` for the Hillvue
installation. Weather-related pressure changes affect the result, so it should
be treated as an estimate rather than GPS elevation.

## Air quality

Air quality combines BME680 gas resistance with humidity compensation. Lower
scores represent cleaner air:

| Score | Rating |
| ----- | ------ |
| `0–50` | Good |
| `51–100` | Moderate |
| `101–150` | Poor |
| `151–200` | Unhealthy |
| `201–300` | Very poor |
| `301–500` | Hazardous |

The first ten successful samples establish a clean-air baseline. The baseline,
sample count and readings remain in RAM and restart after every reboot. This is
a relative indicator for observing changes at a fixed installation, not a Bosch
BSEC IAQ measurement.

## Storage

Current readings and the air-quality baseline are never written to flash. The
node identity, configuration and ACL use the normal MeshCore persistent store.
Battery history used by the sensor runtime is also held in RAM.
