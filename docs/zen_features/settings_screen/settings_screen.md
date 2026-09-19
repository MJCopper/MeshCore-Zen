# Settings

[Back to README](../../../README.md)

Settings opens a flat section list. Each section has its own screen. Changes
are staged and saved on Back only when the final value differs from the saved
value.

Zen tracks a separate semantic configuration schema alongside the preference
file layout. Boot applies any outstanding ordered migrations, normalises active
settings and compacts override tables, then saves only when stored data changed.
On a first UF2 upgrade from MeshCore v1.17.1, Zen imports shared settings from
`/prefs.json` before creating defaults. The original file is kept. If Zen has
already booted without those settings, **System › Import MeshCore** offers a
confirmed one-time recovery; it keeps Zen-only options and restarts to apply
the imported radio settings.

## Display

Brightness, display timeout, battery format, clock seconds and 12/24-hour time.
E-ink builds also provide display rotation, joystick rotation and full-refresh
interval.

Battery percentage estimates usable charge from voltage: 0% at 3.3 V and 100%
at 4.12 V, using a non-linear single-cell Li-ion/LiPo discharge curve. The
device shuts down at or below 3.3 V unless externally powered.
At 20% or below, a Low Battery alert sounds one short beep, then repeats hourly
while on battery power. Quiet Time suppresses the beep; the visual notification
follows the message screen-wake rules. The alert does not add an unread message.
After three consecutive battery samples at 5% or below (about 24 seconds), Low
Power mode turns off GPS, Bluetooth and the radio, stops automatic
message retries and uses minimum OLED brightness with a five-second screen
timeout. CardKB remains available while the screen is on and is suspended with
the screen as usual. Low Power remains active despite voltage rebound and
restores the saved GPS, Bluetooth, radio and brightness settings only after
external power is connected.
Interrupted messages remain available for manual resend; they are not sent
automatically after restoration.

The final carousel card in Low Power is **Low Power Emergency**. Enter opens a
Yes/No confirmation for a volatile ten-minute emergency window. The radio and
messaging operate during that window. GPS and Bluetooth start off but can be
temporarily enabled from their carousel cards without changing their saved
settings. The page shows the remaining time. At expiry, GPS, Bluetooth and the
radio turn off, pending automatic retries stop, and normal Low Power restrictions
resume. The card always shows Radio, GPS and Bluetooth status. While active,
Enter opens a confirmation to end Emergency Mode early. The battery display
reaches 0% and shuts down at 3.3 V.

## Notifications

Mode controls notification sound: On sounds even when a phone or USB client is
connected; Off is silent; Auto sounds when no client is connected. Changing
Mode updates its row without a sound or popup, and saves on exit. Messages are
still received and counted in every mode. Eligible message and new-contact
popups appear whenever the display is on, regardless of Mode, Quiet Time or
DND. A source set to Off or blocked by Child Mode has no local alert.

Screen Wake controls a sleeping display: Off never wakes for these alerts; On
wakes when Mode permits a local alert; Always wakes even with Mode Off, a
connected client in Auto, Quiet Time or DND. It defaults to On. A notification
wake lasts five seconds unless a button is pressed. Routine adverts and
acknowledgements do not wake the screen. Low Battery may wake with On or Always,
independently of Mode.
Triple-press Back to toggle RAM-only DND with the same sound policy as Quiet
Time. The crossed-speaker icon shows the default notification-audio state;
Local overrides in Auto and manual previews may still sound. Neither DND nor
the passage of Quiet Time writes preferences to flash.
Low-battery and diagnostic warnings remain visible even when message
notifications are Off. The Low Battery beep follows Mode, Quiet Time and DND.

## Sound

Volume, DM/channel/new-contact/advert melodies and advert sound scope.
**New Sound** defaults to None and applies only when an advert adds a contact
to the local table. It is independent of **AD Sound**, which applies to routine
adverts. Requested Discover results do not play AD Sound. Notification sounds
can use Message, Kerplop, Chime, Ripple, Beacon, Cheer, Orbit, Alert, Custom1,
Custom2 or None. Any sound can be assigned
globally or to an individual contact or channel. Custom melodies are edited in
**Tools › Ringtone Editor**. Each supports up to 16 notes or pauses, chromatic
pitches from C through B, octaves 4–7, four note lengths and five tempos.
Accidentals are shown as sharps; use the enharmonic sharp for a flat note.
Use Left/Right to select a melody and Enter to preview it. Manual previews also
play during Notifications Off, Quiet Time and DND; selecting None remains silent.
Preview does not save the selection; settings save on Back.

## Advert

| Setting | Options |
| ------- | ------- |
| Auto Advert | Off / 1 hour / 3 hours / 6 hours |
| GPS Details | Hide / Share |

GPS Details applies to manual, automatic and companion-triggered self adverts.
The advert status icon appears steadily for five seconds after an advert is
queued.

## Home Pages

Enter toggles a page and Left/Right changes its order. Clock is fixed first;
Messages and Settings remain available. Position 1 is the first page after
Clock.

Default order: Messages, Favourites, Sensors, GPS, Advert, Bluetooth, Radio,
Tools, Settings.

Sensors lists saved sensor nodes. Open a node to fetch its telemetry; Up/Down
scrolls one value at a time, Enter refreshes and Back returns to the list.
Channels appear in descending order, with values kept in their original order
within each channel. Only telemetry labels and values are shown. Requests use
the node's existing path and access permissions. After no reply, Zen tries
blank-password ACL login. Confirmed access retries telemetry automatically;
otherwise Enter offers administrator-password login followed by one retry.
There is no background polling.
Sensors is enabled by default on fresh settings. Enable it here on existing
devices. It is hidden while Child Mode is locked.

See the [Sensors guide](../sensors/sensors.md) for navigation and login behaviour.

## Radio

TX power, preset, frequency, spreading factor, bandwidth and coding rate. Preset
changes apply the frequency, bandwidth, SF and CR together. Frequency uses a
validated digit editor.

**Flood Scope** sets the saved default region for message floods and flood
adverts. Enter a region name (or leave it empty for no scope), confirm the
delivery warning, then leave Settings to save. **Msg Flood** shows whether
message floods currently use the default or a temporary app override.
**Use Default** clears that override in RAM without changing the saved scope.

## System

| Setting | Options or action |
| ------- | ----------------- |
| Name | Up to 31 characters |
| Time Zone | Opens the City / Fixed UTC editor |
| GPS | On / Off |
| GPS Polling | Continuous / Adaptive / 2 min / 5 min / 15 min / 30 min / 1 h |
| Units | Metric / Imperial |
| Reboot | Save pending changes and restart |

Timed GPS polling acquires a stable fix, caches it, powers down, then repeats
after the selected interval. GPS and Bluetooth changes are applied when
Settings is closed. See [GPS and Course](../gps/gps.md) for acquisition timing,
power use, fix quality and the direction display.

City mode uses the selected city's standard and daylight-saving rules. Fixed UTC
uses an unchanging UTC−12:00 to UTC+14:00 offset in 15-minute steps. The editor
only shows the value relevant to the selected mode and previews the effective
offset. Fresh installations default to City with Sydney selected. Existing
installations retain their previous Fixed UTC offset until City is selected.
City mode does not select a location from GPS. See the
[Time Zone guide](../timezone/timezone.md) for the supported cities and all
local-time consumers.

## Bluetooth

| Setting | Options or action |
| ------- | ----------------- |
| Bluetooth | On / Off |
| PIN Mode | Random / Fixed |
| PIN | Edit the fixed six-digit pairing PIN |

Random mode generates a new pairing PIN each time Zen boots. Fixed PIN changes
are saved on Back and take effect after reboot.

See [Radio, GPS, Bluetooth and Adverts](../connectivity/connectivity.md) for the
related home-page controls and [GPS and Course](../gps/gps.md) for detailed GPS
behaviour.

## Keyboard

Choose predictive **T9** (default) or **ABC**. Literal fields use multi-tap even
with T9 selected. **CardKB** is a read-only `Found`/`Missing` status.
To enter `0` in T9, switch to symbols and press the bottom-right key once.

Word completion is limited to messages. Its 4,000-word Australianised
dictionary excludes proper names, fragments and unsuitable terms. The smiley
key opens the four-entry emoji picker.

## Contacts

Direct messages, Channels and Rooms can each show **All** or **Favourites**.

## Child Mode

Set the six-digit PIN, enable the mode, and select whether favourited Rooms,
favourited private Channels and the Favourites home page remain visible. See
[Child Mode](../child_mode/child_mode.md).

## Quick Replies

Ten natural built-in replies are always available. Edit up to five additional
custom replies here; empty custom slots are omitted from the reply picker.
Custom replies support the same time, location and active-sensor placeholders
as message composition.

See the [Quick Replies guide](../quick_replies/quick_replies.md) for use from a
conversation.
