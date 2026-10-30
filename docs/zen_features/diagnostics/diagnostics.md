# Diagnostics

[Back to README](../../../README.md)

Open **Tools › Diagnostics** and use Left/Right to change tabs.

| Tab | What to check |
| --- | ------------- |
| Live | Radio, mesh and queue counters; hold Enter to reset |
| Signal | Recent signal samples; select one and press Enter for details |
| Battery | Voltage, charge estimate and remaining runtime |
| Location | GPS state, fix age, satellites and next search |
| Power | Whether requested display, keyboard, GPS, Bluetooth and radio changes have been applied |
| System | Device, firmware, flood scopes and storage usage |
| Events | The 16 latest warnings and failures |
| Font | Display characters and glyphs |

On **Events**, use Up/Down to select an entry and Enter to read it. Consecutive
identical events are combined. Hold Enter clears the log; reboot also clears
it. Repeated background failures are logged without repeatedly waking the
screen.

Storage is sampled when Diagnostics opens, not continuously. **LOW** is an
advisory warning that there may not be enough space for a replacement file;
**Unavailable** means usage could not be measured. An unreadable filesystem
stops normal boot before default settings can be saved over it. Zen does not
format the filesystem to recover space. Back up identity,
contacts, channels and settings with the companion app; Zen's on-device
message transcripts are RAM-only and are not part of that backup.

**Msg flood** shows whether messages use the saved default region, an app
override or no scope. **Advert flood** shows the saved default for flood
adverts. Neither is a signal-strength or direct-message status.
