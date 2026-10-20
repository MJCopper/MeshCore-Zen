# MeshCore 1.18 integration and acceptance record

Release baseline: `81dbae592ffad40b4212bc0b37e82b38da08f4bc`, shared by official
`companion-v1.18.0`, `repeater-v1.18.0` and `room-server-v1.18.0` tags.
Validation date: 20 October 2026 (Australia/Sydney).

Status: software integration and automated validation recorded below. Hardware
acceptance remains pending. This record does not claim every physical feature
has been tested or authorize a production release.

## Recovery and branch isolation

The local `meshcore/main` branch was fast-forwarded from `d9296435` to the release.
The three product branches independently merge that baseline without rebasing
or force-pushing. Backup refs retain each original product:

| Branch | Original commit | Backup ref |
| --- | --- | --- |
| meshcore/main | d9296435 | backup/pre-1.18-meshcore-main |
| zen/main | e35bceea | backup/pre-1.18-zen |
| meshcore/sensor | 90f9cbcb | backup/pre-1.18-sensor |
| meshcore/archiver | 981c5872 | backup/pre-1.18-archiver |

Device rollback requires a saved settings backup and matching previous firmware.
Do not assume newer preference files can be read by older firmware. The archive
history is intentionally volatile and cannot survive an upgrade/reboot.

## Integration decisions

- Zen keeps its presentation, message histories, child-mode permissions,
  notifications, power policies and UI callbacks. The 1.18 listener extension
  API coexists with this context-rich UI binding. The binding is explicitly
  retained in the boot composition; replacing it with an unregistered listener
  would silently disable standalone features.
- Zen incorporates protocol version 14, local and permitted remote CLI,
  unsolicited binary responses (including telemetry push), multi-byte raw paths,
  updated GPS diagnostics and board lifecycle/dynamic preferences. Remote CLI
  cannot execute while Child Mode is restricted. CLI timezone changes select
  Zen's fixed-offset mode so clock, quiet time and pet schedules agree.
- Zen retains repeater Yield x2 and uses the new configurable transmit-delay
  factors. The new baseline preference fields participate in save suppression;
  dynamic preference dirty state also forces a baseline save.
- Sensor retains its application policy hooks, guest telemetry, admin ACL
  persistence, leaf policy, environmental transformations and public bot. The
  1.18 sensor runtime supplies subscriptions, region loading and board hooks.
  Subscription thresholds are bounds-checked before state mutation; humidity
  keeps its one-byte, half-percent encoding. Push telemetry reserves its extra
  timestamp bytes. Custom serial commands use the updated runtime signature.
- Archiver retains the Public decoder, RAM ring, room snapshot boundaries,
  hop-dependent history limit, read-only policy and ACK-driven synchronization.
  Production snapshot selection was extracted into a directly tested policy;
  normal room-server builds remain free of archive policy.
- Baseline sources remain exactly upstream in Zen's protected paths. Reviewed
  overlay counterpart hashes and the sensor boundary pin now reference 1.18.
  PlatformIO environments, firmware filenames and CI branch tests are retained.

## Automated validation

| Check | Result |
| --- | --- |
| Official baseline native + KISS tests | 54/54 passed |
| Official Wio repeater and room-server builds | Both passed |
| Zen native + KISS tests | 485/485 passed |
| Sensor native + sensor + ASAN + KISS tests | 326/326 passed, including repeated instrumented cases |
| Archiver native + KISS tests | 63/63 passed |
| Zen baseline/shadow boundary | Passed against pinned 1.18 SHA |
| Sensor boundary | Passed against pinned 1.18 SHA |
| Strict documentation build | Passed on all three product branches |
| Both Zen firmware builds | Passed; version v2.2.36 |
| Standard Wio BLE companion boundary build | Passed |
| Custom sensor and standard sensor boundary builds | Passed |
| Custom archiver and standard XIAO room-server boundary builds | Passed |

The sensor sanitizer initially could not link because the host's libasan runtime
was absent. The matching Fedora runtime was downloaded/extracted under `/tmp`
and the complete instrumented suite was rerun successfully using `LIBRARY_PATH`
and `LD_LIBRARY_PATH`. No system packages were installed. Stalled dependency
clones were replaced with copies of the verified local RadioLib dependency at
`6d8934836678d8894e3d556550475b37dce3e2b6`.

## Feature-preservation and hardware acceptance register

Existing custom source changes were compared to the 1.17.1 baseline and the
upstream 1.18 changes. The following groups are retained/adapted. Automated
coverage is necessary but does not replace the pending device checks.

| Product/group | Automated coverage | Pending physical acceptance |
| --- | --- | --- |
| Zen messaging/transcripts | delivery, ACK tracker, text, unread, channel slots, remote operations | direct/channel/room send/receive, resend, fallback, echo counts, duplicates, app coexistence |
| Zen UI/input | UI controls, editor, CardKB, word/CLI completion, UTF-8, pet UI | every page/menu, button hold/repeat, keyboard connect/disconnect, ABC/T9/emoji and favourites on both displays |
| Zen time/location | GPS modes/adaptive/course, deadlines, boot sync, timezone/DST | cold/warm GPS, stale fixes, RTC/reboot, companion time, city/fixed offsets and all GPS schedules |
| Zen notifications | notifications, quiet time, melodies, ringtone models | wake, sounds/vibration, per-source overrides, midnight/DST transitions and Bluetooth Auto mode |
| Zen access/admin | admin session/commands, login, remote operations, Zen policy | PIN restrictions, favourite-only messaging, local/remote CLI permissions, discovery/hidden nodes, ping/path/admin read-back |
| Zen power/radio | battery, Bluetooth, peripheral power, repeater timing/signal, relay timing | battery calibration, 5% suspension, override, 3.3 V shutdown, radio/GPS/BLE recovery and RF timing |
| Zen persistence/diagnostics | codec migrations/boundaries, runtime/config maintenance, storage, diagnostics/trace | upgrade real saved state, backup/restore, interrupted writes and hardware diagnostics |
| Zen pet | care/evolution, training, personality, rewards, UI/display, notifications, persistence/filesystem | retained pet save, physical animations/audio, sleep/wake, retirement and interaction under traffic |
| Sensor measurements | BME680 air quality, cached telemetry, policy | actual sensor values/units, 60 s cadence, ten-sample warm-up, +100 m offset, missing sensor and I2C recovery |
| Sensor public bot | parser, response queue, policy and encrypted mesh integration | every command/alias on Public and #hillvue, rate/duplicate limits, two-part timing, channel isolation and no unsolicited posts |
| Sensor traces | probe, route candidates/history, network/reliability/integration | direct through ten repeaters, hash widths, congestion, late results, retries, failures and bounded reporting |
| Sensor access/storage | management policy, storage, network tests | old identity/ACL/password/radio settings retained, guest reads/admin writes, forced leaf policy, new subscriptions/expiry/unsubscribe |
| Archiver capture | Public decoder text/type/length rejection | actual encrypted Public capture, other-channel rejection, passive RF behaviour |
| Archiver history | ring wrap, snapshot floor/cutoff/sync_since and encoded hop policy | 256/257+ entries, direct/one-hop/full vs multi-hop/20, concurrent sessions, wrap during sync |
| Archiver access/delivery | upstream ACL/protocol helpers and standard/custom compile boundary | all login roles, post rejection including admin, ACK pacing, three failures, 12 clients and reboot-cleared history |

## End-to-end release gate (pending)

Use OLED and E-ink Zen devices, the BME680 sensor, the archive, an official 1.18
companion and a 1.17.1 peer. Exercise direct, one-hop and two-plus-hop routes:

- Poll and subscribe from a companion while sensor bot/trace traffic runs;
  receive pushed telemetry, unsubscribe and verify expiry.
- Synchronize archive snapshots from Zen and official companions, including
  messages arriving during synchronization and all access roles.
- Verify old saved identities, contacts, channels, ACL, Zen preferences and pet
  records; export and restore settings before/after flashing.
- Introduce radio congestion, lost ACKs, disconnect/reconnect, reboot and low
  power; verify recovery and bounded memory/queues.
- Run at least 24 hours across a quiet-time boundary and archive wraparound;
  retain logs, memory readings, traffic observations and device identifiers.

No identifiable supported USB firmware devices were available; the host exposed
only generic ttyS entries. Physical display, RF,
GPS, sensor, battery, OTA, mixed-firmware interoperability and soak tests are
therefore PENDING. Promote/release only after recording those results.

## Host test suites available on this branch

- `test_archive_session`
- `test_companion_118`
- `test_companion_node_prefs`
- `test_config_serializer`
- `test_kiss_modem`
- `test_mesh_tables`
- `test_public_archive`
- `test_routing_policy`
- `test_utf8_helpers`
- `test_utils`

## Artifact verification

[Artifact checksums and UF2 block counts](meshcore-1.18-artifacts.json) were
recorded after validating UF2 magic words, family ID, block sequence/count,
payload sizes and embedded firmware version. Sensor/archive DFU ZIP manifests
and ZIP integrity were checked. Paths under `/tmp` identify the isolated local
worktrees used for those branch builds. These checks do not replace flashing.

The branch validation evidence is retained as [test results](validation/test-results.log),
[firmware builds](validation/firmware-build.log) and
[documentation build](validation/documentation-build.log).
