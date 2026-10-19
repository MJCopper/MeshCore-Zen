# Getting Started

[Back to README](https://github.com/MJCopper/MeshCore-Zen/blob/meshcore/sensor/README.md)

## Before powering the node

Confirm that the XIAO nRF52840, SX1262 and BME680 share a common ground. The
BME680 logic must operate at 3.3 V. Connect it to `D6` for SCL and `D7` for SDA;
do not use the XIAO's usual `D4`/`D5` I2C pair because those pins are assigned to
the radio in this firmware.

Review the complete [wiring table](./sensor_features/hardware/hardware.md) before
connecting USB or another power source.

## Install the firmware

1. Build or obtain `Xiao_nrf52_bme680_sensor.uf2`.
2. Connect the XIAO to the computer with a USB data cable.
3. Quickly press **Reset** twice to mount the bootloader drive.
4. Copy `Xiao_nrf52_bme680_sensor.uf2` to the drive and wait for the XIAO to
   restart.

The detailed procedure and clean-install notes are in the
[README](https://github.com/MJCopper/MeshCore-Zen/blob/meshcore/sensor/README.md#flashing).

## Configure the node

1. Discover `BME680 Sensor` from a MeshCore companion.
2. Open the node's management action and log in with the administrator password.
3. Change the default password, set a recognisable node name and apply the radio
   parameters used by the local mesh.
4. Request telemetry after the first one-minute sampling interval.

Air quality needs ten successful samples to establish its RAM-only baseline.
During that period, telemetry is available but the channel bot reports
`Air Quality: Warming Up (n/10)`.

## Test the Public-channel bot

Send this message on Public, or add the hashtag channel `#hillvue` in your
companion app and send it there. The response stays on the requesting channel:

```text
!hillvue
```

The node returns all available measurements in at most two messages. Up to four
commands are accepted per rolling minute across all users. See
[Public-channel Commands](./sensor_features/public_bot/public_bot.md) for the
selection syntax.
