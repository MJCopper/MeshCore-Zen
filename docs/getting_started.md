# Getting Started

[Back to README](../README.md)

## Before installing

Zen supports the Seeed Wio Tracker L1 with either the OLED or E-ink display.

> [!WARNING]
> Always export a fresh settings backup with the MeshCore companion app before
> installing or updating firmware. Store it off the device and follow the
> complete [flashing instructions](../README.md#flashing).

Choose the UF2 matching the display. After flashing, verify the identity,
radio settings, contacts and channels, and confirm persistence after a
reboot.

## First setup

1. Set the device name, selected city or fixed UTC offset, and units under
   **Settings › System**. City mode adjusts daylight saving automatically but
   does not detect the city from GPS; see the [Time Zone guide](./zen_features/timezone/timezone.md).
2. Select the local mesh preset under **Settings › Radio**.
3. Enable Bluetooth if a phone or computer will be used as a companion.
4. Send an advert from the Advert home page.
5. Choose saved GPS power and a polling interval under **Settings › System**.
   Leave GPS enabled until it obtains a fix if location sharing is required.

The clock shows `SYNC TIME` until valid time arrives from GPS or a connected
companion. Radio and messaging continue while time is unsynchronised.

## Navigation

- **Left/Right** moves between home pages or changes a setting value.
- **Up/Down** moves through lists and scrolls transcripts.
- **Enter** opens or confirms the selected item.
- **Hold Enter** opens available actions or quick replies.
- **Back** cancels or returns to Clock from another home page. On Clock, it
  turns the display off.

Back is the only button that wakes a sleeping display. A non-Clock home page
returns to Clock after five minutes without input.

## Companion connection

Enable Bluetooth from its home page or under **Settings › System**. While the
device is waiting to pair, the Bluetooth page displays its PIN. BLE takes
care of companion-app communication; USB is reserved for charging and DFU
flashing.

Settings are normally staged while editing and written only when leaving the
section with a changed value.

Timed GPS modes power the receiver only while acquiring a fix. The GPS page
shows its current state, last-fix age and course-over-ground. See
[GPS and Course](./zen_features/gps/gps.md) for polling, power and direction
behaviour.
