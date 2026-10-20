# Hardware and Wiring

[Back to README](https://github.com/MJCopper/MeshCore-Zen/blob/meshcore/archiver/README.md)

The archive target is built for a Seeed Studio XIAO nRF52840 and an SX1262 LoRa
module. There is no display, keyboard or external message-storage device.

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
is not assigned to a separate XIAO pin. Confirm that the selected radio module
supports this arrangement before applying power.

The configured transmit-power range permits up to 22 dBm. Connect a suitable
antenna before transmission and use radio settings permitted in the operating
region.

## Power and indicators

The node can run continuously from USB or a suitable battery supply. The XIAO's
red LED is used as the LoRa transmit indicator and briefly illuminates when the
radio sends.

The standard XIAO nRF52840 battery-voltage measurement, DC/DC support and
low-voltage protection remain enabled. All radio and logic signals use 3.3 V
levels.

## Unused connections

`D6` and `D7` are configured as the target's I2C pins but the archive firmware
does not require an I2C peripheral. The user button and null display driver are
provided by the XIAO target; the archive has no local menu or message display.
