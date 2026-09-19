# Diagnostics

[Back to README](../../../README.md)

Open **Tools › Diagnostics** and use Left/Right to change tabs.

- **Live** shows radio, mesh and queue counters. Hold Enter to reset them.
- **Battery** shows voltage, curve percentage and learned remaining runtime.
- **System** shows device and firmware information, saved and effective flood
  scopes, and separate internal and contacts/channels flash usage.
- **Events** contains the 16 newest RAM-only warnings and errors.
- **Font** provides a glyph viewer for display testing.

On Events, use Up/Down to select an entry and Enter to read its complete text.
Consecutive duplicates are combined. Hold Enter to clear the log; reboot also
clears it. Events are never written to flash. Repeated background failures are
rate-limited to avoid excessive display wakeups and noise.

Storage is sampled when Diagnostics opens, not continuously. **LOW** is an
advisory warning that there may not be enough space for a replacement file;
**Unavailable** means usage could not be measured. An unreadable filesystem
stops normal boot before default settings can be saved over it. Zen does not
format the filesystem to recover space. Back up identity,
contacts, channels and settings with the companion app; Zen's on-device
message transcripts are RAM-only and are not part of that backup.

**Msg flood** shows the scope used by outgoing message floods: **Default**,
**App override**, **App unscoped** or **Unscoped**. **Advert flood** shows the
saved default used for flood adverts, which is unaffected by a temporary app
override. Neither status describes direct-path messages or signal strength.
