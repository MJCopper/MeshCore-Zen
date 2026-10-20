# Getting Started

[Back to README](https://github.com/MJCopper/MeshCore-Zen/blob/meshcore/archiver/README.md)

## Before powering the node

- Connect an antenna suitable for the configured LoRa frequency.
- Confirm that the SX1262 wiring matches the
  [hardware guide](./archiver_features/hardware/hardware.md).
- Use 3.3 V logic and a suitable USB or battery power source.
- Configure radio parameters that are legal for the installation location.

## Install and configure

1. Build the UF2 using the [build guide](./building_archiver.md).
2. Double-press **Reset** on the XIAO nRF52840.
3. Copy `firmware.uf2` to the bootloader drive.
4. Connect over USB serial and configure the node name, radio parameters and
   administrator password.
5. Send an advert so companions can discover the node as a Room Server.

The clean-build defaults are:

| Setting | Value |
| ------- | ----- |
| Name | `Public Archive` |
| Administrator password | `password` |
| Stored messages | 256 |
| Maximum clients | 12 |
| Packet forwarding | Off |

Saved settings survive normal firmware updates and override build defaults.

## Connect from a companion

- Log in with the administrator password for remote management.
- Log in with the guest password or leave the password blank for the limited
  Guest room interface.
- Opening the room starts a snapshot synchronization. Newly received Public
  messages are held for the next login rather than pushed immediately.

Archive history is cleared whenever the node restarts or loses power.
