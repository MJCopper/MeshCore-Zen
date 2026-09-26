# Zen persistence boundary

MeshCore owns all standard companion data. Zen compiles MeshCore's original
`DataStore.cpp`, `DataStore.h` and `NodePrefs.h` directly and does not maintain
a replacement implementation.

| Owner | Data |
|---|---|
| MeshCore `IdentityStore` | Device identity |
| MeshCore `DataStore` | `/prefs.json`, contacts, contact paths, channels and advert blobs |
| Bluefruit | Bluetooth bonds under `/adafruit` |
| Zen `ZenStore` | External `/zen_prefs` and Zen runtime records |
| RAM | Drafts, Silent mode and temporary operation state |

## Baseline data

The phone app and the on-device interface share the same live MeshCore
`NodePrefs`. Radio, node, GPS, telemetry, advert privacy, Bluetooth PIN and
scope changes are saved only through MeshCore's `DataStore::savePrefs()` and
`NodePrefs::saveSerial()`.

Companion commands mutate that live baseline model directly. Zen does not
stage, mirror or defer phone-originated identity, preference, contact or
channel operations. Radio commands apply the saved parameters immediately,
and the companion reboot command uses MeshCore's normal pending-contact flush.

Contacts, nodes, channels and paths use MeshCore's original load and save
functions and binary formats. Zen screens are views and controls over those
objects; they do not keep a second database or rewrite saved paths.

## Zen data

`ZenPrefs` extends the live preference model for UI use, while
`ZenPrefsCodec` serializes only Zen-owned fields. `ZenStore` receives only the
external filesystem and therefore cannot open, validate, migrate or remove
MeshCore files.

Zen-owned settings include:

- Display, homepage and text-entry preferences.
- Favourites and list filters.
- Quick Replies.
- Notification melodies and per-conversation notification choices.
- Child Mode and Quiet Time.
- Timezone presentation and adaptive GPS policy.
- Automatic advert interval and saved radio-preset shortcuts.

The Zen record is checksummed and written using `/zen_prefs.tmp` followed by a
rename. A verified direct-write fallback supports Wio QSPI implementations
that reject replacement renames. Its change fingerprint prevents unchanged
writes. A failure affects only Zen settings and cannot replace MeshCore
preferences, identity, contacts, channels, paths or Bluetooth bonds.

## Save rules

- A baseline field change calls the original MeshCore save path.
- A Zen field change calls `ZenStore`.
- When a screen changes both domains, each domain commits independently.
- Contact and channel changes call the original MeshCore APIs.
- Runtime-only state never writes preferences.
- Existing MeshCore backup and restore behaviour remains authoritative.

This release intentionally does not migrate settings from earlier Zen/Solo
storage layouts. Restore MeshCore data from a companion-app backup and
configure Zen settings again after installation.

On Wio Tracker builds, boot removes obsolete Zen/Solo extension records from
the 28 KB internal filesystem. This preserves working space for MeshCore's
`/prefs.json` and Bluefruit bond records. The cleanup never removes the
identity, current MeshCore preferences, contacts, channels or `/adafruit`
pairing data.

## Build enforcement

The baseline-boundary check fails if the Zen overlay adds replacements for
`DataStore.cpp`, `DataStore.h` or `NodePrefs.h`, or if the build stops compiling
MeshCore's original `DataStore.cpp` directly.
