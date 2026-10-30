# Settings

[Back to README](https://github.com/MJCopper/MeshCore-Zen/blob/zen/main/README.md)

Settings opens a flat section list. Each section has its own screen. Changes
are staged and saved on Back only when the final value differs from the saved
value.

Zen tracks a separate semantic configuration schema alongside the preference
file layout. Boot applies any outstanding ordered migrations, normalises active
settings and compacts override tables, then saves only when stored data changed.
On a first UF2 upgrade from MeshCore v1.18.0, Zen imports shared settings from
`/prefs.json` before creating defaults. The original file is kept. If Zen has
already booted without those settings, **System › Import MeshCore** offers a
confirmed one-time recovery; it keeps Zen-only options and restarts to apply
the imported radio settings.

## Display

Brightness, display timeout, battery format, clock seconds and 12/24-hour time.
The display stays on while on-device messages, discovery, Ping, Trace, remote
login/telemetry/admin requests or pet games are in progress. After the last
operation finishes, the full configured display timeout starts. Manual screen-off
and low-power protection take precedence. Background mesh activity and
companion-app commands do not acquire a hold.
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

| Setting | What it does |
| ------- | ------------ |
| Mode: On | Sounds even with a phone connected |
| Mode: Off | Silences notification sounds |
| Mode: Auto | Sounds when no client is connected |
| Screen Wake: Off | Leaves a sleeping display off |
| Screen Wake: On | Wakes when notification mode permits a local alert |
| Screen Wake: Always | Wakes even when sounds are suppressed |

Messages and unread counts continue in every mode. Permitted popups appear
while the display is on; a source set to Off or blocked by Child Mode has no
local alert. Notification wakes last five seconds unless a button is pressed.

Set **Quiet Time**, **Quiet from** and **Quiet until** here. Triple-press Back
for temporary Do Not Disturb. See [Quiet Time](../quiet_time/quiet_time.md) for
schedule, sound overrides and the crossed-speaker icon.

Screen Wake controls a sleeping display: Off never wakes for these alerts; On
wakes when Mode permits a local alert; Always wakes even with Mode Off, a
connected client in Auto, Quiet Time or DND. It defaults to On. A notification
wake lasts five seconds unless a button is pressed. Routine adverts and
acknowledgements do not wake the screen. Low Battery follows the same wake rule:
On follows Mode, while Always can wake despite Mode, Quiet Time or DND.
Triple-press Back to toggle RAM-only DND with the same sound policy as Quiet
Time. The crossed-speaker icon shows the default notification-audio state;
Local overrides in Auto and manual previews may still sound. Neither DND nor
the passage of Quiet Time writes preferences to flash.
Low-battery and diagnostic warnings remain visible when the screen is on even
when message notifications are Off. The Low Battery beep and vibration follow
Mode, Quiet Time and DND. Diagnostic warnings are silent and use popup priority
so routine status messages cannot immediately replace them.

## Sound

Choose volume and melodies for direct messages, channels, new contacts and
adverts. **New Sound** applies only when an advert adds a contact; **AD Sound**
applies to routine adverts. Discover results do not play AD Sound.

Use Left/Right to choose a melody and Enter to preview it. Previews can sound
during Notifications Off, Quiet Time or DND; None stays silent. Save with Back.
Contact and channel menus can override the global melody.

Edit Custom1 and Custom2 under **Tools › Ringtone Editor**. Each supports up to
16 notes or pauses, with pitch, octave, note length and tempo controls.

## Advert

| Setting | Options |
| ------- | ------- |
| Auto Advert | Off / 1 hour / 3 hours / 6 hours |
| GPS Details | Hide / Share |

GPS Details applies to manual, automatic and companion-triggered self adverts.
The advert status icon appears steadily for five seconds after an advert is
queued.

## Home Pages

Enter toggles a page; Left/Right changes its order. Clock stays first, and
Messages and Settings remain available. Position 1 is the first page after Clock.

Default order: Messages, Favourites, Sensors, GPS, Advert, Bluetooth, Radio,
Tools, Settings. Enable Sensors here on existing devices if it is missing;
it is hidden in locked Child Mode. See [Sensors](../sensors/sensors.md).

## Radio

Choose TX power, a preset, or individual frequency, spreading factor, bandwidth
and coding-rate values. A preset applies its radio values together. Use
Previous/Next to browse built-ins, **+ Save current...** to keep a custom preset,
and **- Delete preset...** to remove one. Up to four custom presets are available.
Built-ins may also apply a recommended path-hash size; custom presets keep
the current size. Installing firmware does not change saved radio settings.

**Flood Scope** sets the default region for message floods and flood adverts.
Enter a name, or leave it empty for no scope, confirm the delivery warning and
leave Settings to save. **Msg Flood** shows a temporary app override, if any;
**Use Default** clears it and returns to your saved scope.

Trace guest (Radio section) enables empty-password public guest introduction for
repeater Trace, On by default. Off retains ordinary discovery and advert-route
fallback. Trace never automatically uses an administrator password.

## System

| Setting | Options or action |
| ------- | ----------------- |
| Name | Up to 31 characters |
| Time Zone | Opens the City / Fixed UTC editor |
| GPS | On / Off |
| GPS Polling | Continuous / Adaptive / 2 min / 5 min / 15 min / 30 min / 1 h |
| Units | Metric / Imperial |
| Zen Pet | On / Off; show the virtual pet with daily/manual checkpoints |
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
with T9 selected. **CardKB** shows Found or Missing.

In T9, use symbols and press the bottom-right key once to enter `0`. The smiley
key opens the emoji picker. See [Messages](../message_screen/message_screen.md)
for word completion and [CardKB](../cardkb/cardkb.md) for keyboard shortcuts.

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
