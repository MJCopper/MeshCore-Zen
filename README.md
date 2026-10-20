# MeshCore Public Archive Firmware

> **First installation:** Read the [flashing notes](#flashing) before connecting
> the archive node to the mesh.

This branch provides a read-only, RAM-based Public-channel archive for a Seeed
Studio XIAO nRF52840 and SX1262 LoRa radio. It is based on **MeshCore v1.18.0**
and presents archived messages through the standard MeshCore room interface.

## Supported hardware

| Component | Requirement |
| --------- | ----------- |
| Controller | Seeed Studio XIAO nRF52840 |
| Radio | SX1262 module wired for the XIAO target |
| Display | None |
| Storage | Internal RAM; no message history is written to flash |
| Power | USB or a suitable 3.3 V/battery supply |

See [Hardware and Wiring](./docs/archiver_features/hardware/hardware.md) for the
radio pinout and electrical assumptions.

## Software features

- Captures text messages from MeshCore's built-in Public channel.
- Keeps the newest 256 messages in a circular RAM buffer.
- Presents a login-time snapshot through the standard room-server interface.
- Allows the full available history for direct and one-hop sessions.
- Limits sessions detected over two or more hops to the newest 20 messages.
- Does not push newly captured messages to connected companions.
- Gives guest-password and passwordless users the limited Guest interface.
- Retains standard administrator login and remote-management support.
- Rejects room posts from every ACL role.
- Starts in leaf-node mode with packet forwarding disabled.

See [FEATURES.md](./FEATURES.md) for the complete firmware behaviour summary.

> [!WARNING]
> Message history exists only in RAM. Restarting, resetting or removing power
> clears the complete archive.

## Flashing

Flashing replaces the installed firmware. Erasing first also removes the saved
node identity, radio settings, ACL and administrator password.

1. Build or obtain `firmware.uf2` for the `Xiao_nrf52_archiver` target.
2. Connect the XIAO nRF52840 directly to the computer with a USB data cable.
3. Quickly press the XIAO's **Reset** button twice. A removable bootloader drive
   should appear.
4. Copy `firmware.uf2` to the root of that drive.
5. Wait for the copy to finish. The drive normally disconnects and the XIAO
   restarts automatically.

For a clean installation, use the nRF52 erase UF2 from the
[MeshCore Flasher](https://meshcore.io/flasher) before copying the archive UF2.

## First setup

1. Connect the SX1262 and antenna using the documented wiring.
2. Flash the archive firmware.
3. Configure the correct regional radio parameters over USB serial.
4. Change the default administrator password, which is `password` on a clean
   installation.
5. Advertise or discover the node as `Public Archive` in a MeshCore companion.
6. Use an administrator login for remote management, or a guest/blank login for
   the limited read-only room interface.

Saved configuration from an earlier installation takes precedence over build
defaults.

## Guides

- [Getting Started](./docs/getting_started.md)
- [Hardware and Wiring](./docs/archiver_features/hardware/hardware.md)
- [Archive Behaviour](./docs/archiver_features/archive/archive.md)
- [Access and Management](./docs/archiver_features/access/access.md)
- [Building the Firmware](./docs/building_archiver.md)

## Development

Archive-specific behaviour is isolated behind `PUBLIC_CHANNEL_ARCHIVE` and the
`Xiao_nrf52_archiver` PlatformIO environment. The standard XIAO room-server
target remains unchanged.

Build the flashable firmware with:

```sh
pio run -e Xiao_nrf52_archiver
pio run -e Xiao_nrf52_archiver -t create_uf2
```

Mesh routing and the room protocol are provided by
[MeshCore](https://github.com/meshcore-dev/MeshCore).
