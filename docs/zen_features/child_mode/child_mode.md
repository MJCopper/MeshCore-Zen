# Child Mode

[Back to README](../../../README.md)

Child Mode provides a PIN-protected interface with favourite-only messaging.
It is a UI lock, not protection against someone who can erase or replace the
firmware.

## Setup

1. Configure the device and favourite the permitted contacts and rooms.
2. Favourite any permitted private channels.
3. Open **Settings › Child Mode**, set and confirm a six-digit PIN.
4. Choose whether Rooms, Channels and the Favourites page are visible.
5. Enable Child Mode, accept the warning, then leave Settings.

| Setting | Default | Behaviour |
| ------- | ------- | --------- |
| Enabled | Off | Enables restrictions after confirmation |
| Set PIN | — | Sets or replaces the hidden six-digit PIN |
| Channels | Off | Shows favourited private channels only |
| Rooms | Off | Allows favourited room servers; every participant in an enabled room can message the device |
| Favourites | On | Shows the Favourites Dial |

While locked:

- Direct Messages lists only favourited Chat contacts. Rooms are available only
  when the parent enables Rooms, and then only favourited room servers appear.
  Channels are available only when enabled, favourited and privately keyed.
- Favourite, contact and channel editing is blocked.
- Other messages remain available to the baseline companion protocol but are
  not copied into Zen's on-device history, do not alert or wake the display,
  and do not count as child-visible unread messages.
- Bluetooth and USB companion access are disabled.
- Parent-controlled Settings, Tools, Radio, GPS and Advert pages are hidden.
- Mesh routing and acknowledgements continue normally.
- Automatic adverts continue on their configured schedule and use the existing
  Advert GPS privacy setting.

Repeated incorrect PIN entries are delayed in RAM after the first three tries.
The delay clears after a successful unlock or restart. Opening Settings asks
for the PIN. A successful entry restores parent access
until Settings is closed, the display sleeps or the device restarts.

> [!WARNING]
> If you forget the PIN, the device must be **ERASED & REFLASHED**. Erasing also
> removes identity, contacts, channels, messages and settings.
