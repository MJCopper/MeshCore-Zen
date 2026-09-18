# MeshCore Zen Companion Firmware

> **First installation:** Read the [flashing notes](#flashing) before installing
> Zen, especially if the Wio Tracker is currently running different firmware.

Zen extends the official [MeshCore](https://github.com/meshcore-dev/MeshCore)
companion firmware with a standalone messaging interface for the Seeed Wio
Tracker L1.

Current release: **Zen v1.32.103**, based on **MeshCore v1.17.1**.

## Supported hardware

| Device | Display | Release file |
| ------ | ------- | ------------ |
| Wio Tracker L1 OLED | 128 × 64 SSD1306/SH1106 | `WioTrackerL1_Zen_OLED.<version>.uf2` |
| Wio Tracker L1 E-ink | 250 × 122 GxEPD2 | `WioTrackerL1_Zen_E-INK.<version>.uf2` |

Both builds support BLE and USB serial. Firmware is available from the
[releases page](https://github.com/MJCopper/MeshCore-Zen/releases).

## Features

- Direct, channel and room messaging with full transcripts and delivery state.
- Route-aware retries, manual resend and unread-message shortcuts.
- Predictive T9 and ABC text entry with Australianised, previous-word-aware completion.
- UTF-8 text, monochrome emoji display and four insertable chat emoji.
- Clock, unread shortcut, GPS time sync, power-aware polling and GPS course-over-ground.
- Starred contacts, rooms and channels, plus an editable four-contact speed dial.
- Automatic CardKB support through the Grove I2C port.
- PIN-protected Child Mode, configurable notifications, RAM-only DND and scheduled Quiet Time.
- On-device radio, advert, notification and home-page settings.
- Low-battery protection, emergency mode and learned runtime estimation.
- Node discovery, companion repeater mode, ringtone editor and RAM event log.
- Remote repeater, room and sensor administration from the Node List action menu.
- Sensors carousel with on-demand remote telemetry.

See [FEATURES.md](./FEATURES.md) for the complete Zen feature summary.

> [!WARNING]
> Zen is a personal project and beta-test software. It may contain faults or
> behave unexpectedly. Back up your device and use this firmware at your own
> risk.

## Flashing

For a first installation, back up anything you want to keep. A normal UF2
upgrade from MeshCore v1.17.1 keeps the existing filesystem; Zen imports its
shared settings from `/prefs.json` on first boot. Do not erase the device if
you want to keep its identity, contacts, channels and messages. If an erase is
needed for another firmware or a damaged filesystem, use the
[MeshCore Flasher](https://meshcore.io/flasher), then restore your backup.
Erasing removes the stored identity, contacts, channels, messages and settings.

1. Download the correct `.uf2` for your OLED or E-ink Wio Tracker from the
   [releases page](https://github.com/MJCopper/MeshCore-Zen/releases).
2. Connect the Wio Tracker directly to your computer with a USB data cable. A
   charge-only cable will power the device but cannot transfer the firmware.
3. Quickly press the Wio Tracker's **Reset** button twice. A new removable USB
   bootloader drive should appear on the computer.
4. Copy the downloaded `.uf2` file onto the bootloader drive. Do not copy the
   ZIP file or place the UF2 inside a folder on the drive.
5. Wait for the copy to finish. The bootloader drive normally disconnects and
   the Wio Tracker restarts automatically; this indicates that flashing has
   completed.
6. If the bootloader drive does not appear, check that the cable supports data,
   reconnect it, and repeat the quick double-press of **Reset**.
7. Check the radio settings after first boot. If you erased the device, use
   your companion app to restore the backup you created before flashing.

If Zen was already installed before settings import was available, open
**Settings → System → Import MeshCore** to restore shared settings from the
original `/prefs.json`. The action asks for confirmation and restarts the
device; Zen-only settings are retained. If the file is absent or invalid, use
your companion-app backup instead.

For later Zen updates, repeat the numbered steps. An erase is normally only
needed when changing from another firmware or when troubleshooting damaged
stored configuration.

BLE takes priority over USB serial. Disconnect BLE before using a USB companion
connection.

## Guides

- [Getting Started](./docs/getting_started.md)
- [Messages](./docs/zen_features/message_screen/message_screen.md)
- [Settings](./docs/zen_features/settings_screen/settings_screen.md)
- [Clock](./docs/zen_features/clock_screen/clock_screen.md)
- [Time Zone](./docs/zen_features/timezone/timezone.md)
- [Favourites Dial](./docs/zen_features/favourites_dial/favourites_dial.md)
- [Radio, GPS, Bluetooth and Adverts](./docs/zen_features/connectivity/connectivity.md)
- [GPS and Course](./docs/zen_features/gps/gps.md)
- [Signal Indicator](./docs/zen_features/signal_indicator/signal_indicator.md)
- [Sensors](./docs/zen_features/sensors/sensors.md)
- [Quick Replies](./docs/zen_features/quick_replies/quick_replies.md)
- [CardKB](./docs/zen_features/cardkb/cardkb.md)
- [Child Mode](./docs/zen_features/child_mode/child_mode.md)
- [Quiet Time](./docs/zen_features/quiet_time/quiet_time.md)
- [Tools](./docs/zen_features/tools_screen/tools_screen.md)
- [Repeater Mode](./docs/zen_features/repeater_mode/repeater_mode.md)
- [Node Administration](./docs/zen_features/node_admin/node_admin.md)
- [Diagnostics](./docs/zen_features/diagnostics/diagnostics.md)
- [Battery and Low Power Mode](./docs/zen_features/power/power.md)
- [Build Zen](./docs/building_zen.md)
- [UI developer guide](./docs/design/zen_ui_framework.md)

Protocol references remain under [`docs/`](./docs/).

## Development

See [Building Zen](./docs/building_zen.md) for local builds and tests. Zen tracks
upstream MeshCore. After cloning, enable the repository's protected README merge
driver once:

```sh
git config merge.ours.driver true
```

Contributions should follow the existing code style and remain focused.

Thanks to [vanous](https://github.com/vanous),
[marczykm](https://github.com/marczykm),
[MarekZegare4](https://github.com/MarekZegare4) for
[MeshCore Solo](https://github.com/MarekZegare4/MeshCore-Solo), and the upstream
[MeshCore contributors](https://github.com/meshcore-dev/MeshCore/graphs/contributors).
