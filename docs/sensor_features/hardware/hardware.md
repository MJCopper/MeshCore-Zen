# Hardware and Wiring

[Back to README](https://github.com/MJCopper/MeshCore-Zen/blob/meshcore/sensor/README.md)

The firmware target is built for a Seeed Studio XIAO nRF52840, an SX1262 LoRa
module and one BME680 I2C sensor. There is no display or local user interface.

## BME680

| BME680 | XIAO nRF52840 | Purpose |
| ------ | ------------- | ------- |
| VIN/3V3 | `3V3` | 3.3 V supply |
| GND | `GND` | Common ground |
| SCL | `D6` | I2C clock |
| SDA | `D7` | I2C data |

The firmware expects I2C address `0x76`. Configure a breakout that defaults to
`0x77` for `0x76` before use. Keep all signal levels at 3.3 V.

`D6` and `D7` are intentional. The radio uses `D4` and `D5`, so the default
XIAO I2C pin pair is unavailable to the BME680.

## SX1262 radio

| SX1262 signal | XIAO pin |
| ------------- | -------- |
| DIO1 | `D1` |
| RESET | `D2` |
| BUSY | `D3` |
| NSS/CS | `D4` |
| RXEN | `D5` |
| SCK | `D8` |
| MISO | `D9` |
| MOSI | `D10` |

The target uses SX1262 DIO2 for RF switching and DIO3 for a 1.8 V TCXO. `TXEN`
is not assigned to a separate XIAO pin. Confirm that these assumptions match the
radio module before applying power.

The firmware permits a configured TX power up to 22 dBm. Use an antenna suitable
for the selected frequency and configure radio parameters that are legal for the
installation location.

## Power and indicators

The node can run continuously from USB power. The XIAO's red LED is used as the
LoRa transmit indicator and briefly illuminates when the radio sends.

Supply voltage is included in telemetry. The standard XIAO nRF52840 battery
measurement and low-voltage protection remain active when a battery is fitted.
