# Flood channel text doubler

Current release: **v1.18.0-doubler.7**, based on MeshCore v1.18.0.

This opt-in repeater and companion firmware adds one best-effort second transmission to
selected flood packets. Repeating is off by default. Mode 1 covers Public, hashtag and private channel
text without knowing their names or keys. `multi.flood` can disable repeating or
extend it to all outgoing flood payload types. Existing routing, region filters
and duplicate suppression decide whether the first transmission is allowed.
Direct routes always retain their existing behaviour.

Companion builds apply an additional local-origin restriction: only packets
originating on that device qualify. They do not add copies to received floods
that the companion forwards, even if its existing forwarding option is enabled.
Mode 1 applies to locally sent Public/private/hashtag channel text; mode 2 also
covers locally originated flood messages, requests, ACKs, adverts and channel
data. This does not turn companion forwarding on or change its existing rules.

Local floods built by `Mesh::sendFlood()` start with zero path entries. Relayed
floods have a path entry appended before TX. The companion policy uses the path
entry count (not the encoded path-length byte) so all supported path hash sizes
work. It also explicitly records packets queued by receive/forward handling, so
relays stay excluded even if their path metadata is unusual or wraps. Those
records are cleared on completion or release; capacity matches the 16-packet
pool, with unexpected tracking overflow suppressing new repeats until reboot.
Raw packets injected by an app with zero path entries qualify; raw packets
with an existing path are conservatively excluded. Direct and zero-hop packets
remain excluded in every mode: `sendZeroHop()` uses a direct-route encoding.

## CLI and saved mode

Commands follow the existing `multi.acks` get/set convention:

```text
get multi.flood
> 0
set multi.flood 2
OK
```

| Value | Eligible second transmissions |
| --- | --- |
| `0` (default) | None |
| `1` | Flood channel text (`PAYLOAD_TYPE_GRP_TXT`): Public, hashtag and private channels |
| `2` | All outgoing flood payload types, including ACKs, adverts, replies and channel data |

Both ordinary and region-scoped floods qualify. Mode 2 does not enable packet
types or routes that baseline firmware otherwise rejects. Each eligible outgoing
packet gets at most one optional additional TX. `multi.acks` remains independent;
its direct ACK/multipart-ACK transmissions are not doubled by any mode.

Only the complete values `0`, `1` and `2` are accepted. Trailing whitespace is
allowed; extra arguments, signed values, decimals and strings such as `1abc`
return an error. Queries and unchanged values do not write flash. These commands
use the normal serial CLI and existing authenticated remote administrator path;
region-import processing still takes precedence while an import is active.

On companions, use the existing `CMD_RUN_CLI_COMMAND` interface (protocol v14)
through a compatible USB/Bluetooth client, or the existing rescue CLI. Ordinary
USB companion operation uses framed commands, not arbitrary terminal text. No
new app opcode, preference field or GUI toggle is introduced. CLI response code
and optional three-character prefixes are preserved. Existing contact permission
checks still control remote radio CLI execution.

Settings are stored only in `/doubler.json`:

```json
{"schema":1,"multi_flood":0}
```

The module does not change `NodePrefs`, `/prefs.json`, legacy `/com_prefs`, or the
baseline preference serializer. Missing settings use mode 0 without a boot-time
write. Corrupt settings fail off and may be repaired with an explicit valid
`set multi.flood` command, including `set multi.flood 0`. Unsupported schemas,
unknown fields, oversized files or read failures fail off and reject edits,
preserving data for compatible firmware. The boot log reports unavailable
settings; `get multi.flood` reports the effective mode.

Changes are written to `/doubler.tmp`, closed, checked by reading the bytes back,
then atomically renamed over `/doubler.json`. Save failure returns an error and
keeps the previous runtime mode and valid file. A leftover temporary file is
ignored at boot. This relies on the nRF52 LittleFS backend's atomic rename;
physical power-loss testing is still required on hardware.

Setting mode 0 cancels waiting copies on the next scheduler pass. Changing 2 to
1 cancels waiting non-channel copies. Other retained copies keep their original
ready/expiry times. An active TX always finishes, preserving its repeat marker
so it cannot cause a third transmission. New eligibility applies at first TX
completion, without replaying packets that have already been released.

Ineligibility is latched immediately when a mode changes: `1 -> 0 -> 1` cannot
revive a waiting copy even if no scheduler pass occurred between the commands.
Likewise `2 -> 1 -> 2` cannot restore a non-channel copy cancelled by mode 1.
Queries, unchanged values and failed saves do not cancel eligible copies.

## Operating-context changes

Runtime-only context observations cancel pending copies when applied frequency,
bandwidth, SF or CR changes, or when the active identity, path-hash mode or
default/global scope key changes. An in-memory generation records each change,
so changing A -> B -> A cannot revive old copies.

A TX-start hook captures the generation of each successfully started original.
If its context changes while it is on air, its completion cannot create a copy
on the new profile. Repeats have an explicit in-flight flag and are never freed
or aborted by cancellation, even if cleanup runs without an active-packet
argument. Their marker survives until completion or failure, preventing a third TX.

Radio observations are wired immediately before the actual `setParams` calls:
startup and applied app radio settings for companions; startup, delayed temporary
settings and restoration for repeaters. CLI settings marked "reboot to apply"
do not alter this applied-radio snapshot. Packet-context observations run at
scheduling boundaries and after relevant saved settings/identity/scope changes.
Unrelated saves and USB/Bluetooth connection changes do not invalidate copies.
Per-message scope overrides are preserved in each packet; sending another message
with a different override does not cancel existing copies.

The policy also exposes explicit availability invalidation for a resumable
radio-disable integration. Availability is not inferred from normal RX/TX/busy
states. Current profiles have no resumable LoRa-off mode: system shutdown/reboot
already discards runtime pointers. No Zen power framework or radio stop/reset is
added by this feature. Ordinary queued originals keep baseline handling.

These changes do not add fields to `/doubler.json` or baseline preferences.
Context snapshots, generations and cancellation/in-flight flags are never saved.

Ordinary baseline preference saves do not touch the module file. A stock-firmware
round trip can preserve it when the filesystem is retained; formatting erases
it. Future firmware that reformats storage cannot be guaranteed to preserve it.

## Build and artifacts

The four custom environments inherit the upstream hardware/transport profiles:

| Hardware | Doubler environment | Upstream parent |
| --- | --- | --- |
| Seeed Studio Xiao nRF52 WIO | `Xiao_nrf52_repeater_doubler` | `Xiao_nrf52_repeater` |
| RAK WisBlock / WisMesh RAK4631 | `RAK_4631_repeater_doubler` | `RAK_4631_repeater` |
| USB Seeed Studio Xiao nRF52 WIO companion | `Xiao_nrf52_companion_radio_usb_doubler` | `Xiao_nrf52_companion_radio_usb` |
| Bluetooth Seeed Studio Xiao nRF52 WIO companion | `Xiao_nrf52_companion_radio_ble_doubler` | `Xiao_nrf52_companion_radio_ble` |

Build and verify UF2s and OTA upgrade ZIPs for all four with Python 3.9+ and PlatformIO installed:

```sh
python3 variants/doubler/build.py
```

Pass `--pio /path/to/pio` if needed. Outputs are:

```text
.pio/build/Xiao_nrf52_repeater_doubler/Xiao_nrf52_repeater_doubler.1.18.0-doubler.7.uf2
.pio/build/RAK_4631_repeater_doubler/RAK_4631_repeater_doubler.1.18.0-doubler.7.uf2
.pio/build/Xiao_nrf52_repeater_doubler/Xiao_nrf52_repeater_doubler.1.18.0-doubler.7.ota.zip
.pio/build/RAK_4631_repeater_doubler/RAK_4631_repeater_doubler.1.18.0-doubler.7.ota.zip
.pio/build/Xiao_nrf52_companion_radio_usb_doubler/Xiao_nrf52_companion_radio_usb_doubler.1.18.0-doubler.7.uf2
.pio/build/Xiao_nrf52_companion_radio_usb_doubler/Xiao_nrf52_companion_radio_usb_doubler.1.18.0-doubler.7.ota.zip
.pio/build/Xiao_nrf52_companion_radio_ble_doubler/Xiao_nrf52_companion_radio_ble_doubler.1.18.0-doubler.7.uf2
.pio/build/Xiao_nrf52_companion_radio_ble_doubler/Xiao_nrf52_companion_radio_ble_doubler.1.18.0-doubler.7.ota.zip
.pio/build/doubler-manifest.json
```

The script checks every UF2 block, its nRF52840 family identifier, and records
artifact checksums, the Git commit and whether the working tree has changes.
OTA ZIPs are the application-only Nordic DFU packages generated by the stock
nRF52 build, copied without repacking. The script verifies ZIP integrity, the
embedded firmware version, application CRC16, init packet, and board-specific
SoftDevice requirement (Xiao: `0x0123`; RAK4631: `0x00B6`). Use the package for
your board with its compatible Nordic DFU bootloader; an actual OTA transfer
remains a hardware acceptance check.

The normal upstream environments and release builder are unchanged. Direct
`pio run -e <doubler-environment> -t buildprog -t create_uf2` also works but uses
upstream's default `firmware.uf2` name unless `UF2_FILE_PATH` is supplied.

The separate `doubler.yml` GitHub Actions workflow runs the tests and the same
build script on pushes to `meshcore/doubler` or manual dispatch, then uploads the
versioned UF2s, OTA ZIPs and manifest as workflow artifacts. It does not publish a release.

Custom builds default to Australia Narrow: 916.575 MHz, BW 62.5 kHz, SF7, CR 4/7.
Existing saved radio preferences override these defaults. Check the actual radio
settings before testing; flashing does not reset a node's saved preferences.

Companion profiles retain their upstream 708,608-byte application limit,
`extrafs` linker script, external contacts/channels storage, contacts/channel
limits and USB/Bluetooth configuration. The module settings use
`DataStore::getPrimaryFS()`, not secondary contacts/channels storage. Switching
roles on the same hardware may carry the saved mode if primary storage survives;
the default remains off only when the module file is missing.

## Timing and overload behaviour

After a successful **local** first TX completion, a fresh random delay of
500–800 ms inclusive is chosen. This does not confirm reception by another node.
The same packet is retained, preserving its header, payload, path and transport
codes byte for byte. It does not pass through flood routing a second time.

The gap is nonblocking: receive processing and other queued traffic continue.
The repeat becomes eligible after the chosen delay, with a fixed expiry at
**3,000 ms after the first local TX completion**. Waiting never refreshes that
deadline. Expired waiting packets are freed even while the radio is transmitting
another packet or the airtime budget is exhausted. A second transmission may
start before expiry and finish afterward; an active transmission is never aborted.

Waiting repeats live in the module's own bounded list, outside the normal
outbound queue. Normal packets that are due always go first. When only a repeat
is ready, busy reception or insufficient budget simply defers it without changing
the shared CAD/backoff timers. Optional-only busy or budget waits have their own
200 ms retry pause, avoiding continuous CAD scans without delaying normal
packets or refreshing the expiry. An optional repeat never uses the baseline's
busy-channel timeout to force a transmission. A final check drops the repeat if
it has expired or reception has begun during TX preparation. Normal packets keep
the baseline's channel and force-send behaviour. Both completed transmissions
are logged, counted, and charged to the normal TX budget.

The repeater's 32-packet pool and companion's 16-packet pool are protected by limiting pending repeats to four
and scheduling them only when at least four free slots remain. Under pressure
the optional repeat is skipped; sustained normal traffic makes waiting repeats
expire. These held packets use pool slots but are not included in the normal
outbound queue-length statistic.
Companion pending-work detection includes these optional packets, preventing a
sleep decision from hiding work outside the normal queue. The radio scheduler
continues independently of the app interface; app-level message submissions,
ACK tracking and retries are not invoked again for the optional copy. Each
separate application retry can independently qualify for one extra transmission.
This is best effort, not a promise that every eligible packet reaches the air
twice. Failed starts and timed-out transmissions are released without additional
attempts. A repeat never schedules a third transmission.

Stock 1.18 repeaters recognise the identical copy through normal duplicate
suppression. Their shared cache retains 160 hashes by insertion order, not a
fixed timeout. A restart or sufficient intervening insertions can still allow a
repeat to be processed again; three seconds is not a remote-cache guarantee.
A second TX also consumes
power and creates another interval when this repeater cannot receive.

## Integration and future upstream merges

The policy is in `src/helpers/FloodTextDoubler.h`. Its four pending records hold
the packet pointer, ready time, expiry and lifecycle flags. These are local bookkeeping only:
no Packet fields, wire format, hash, persistent settings
or routing logic are changed.

The only core integration points are:

1. `Dispatcher::retainAfterTransmit(Packet*, uint32_t completed_at)`: called after successful TX
   accounting and logging, before release. Returning true transfers ownership to
   the hook; its default returns false.
   The timestamp is sampled before logging, so logging time does not extend the
   selected delay.
2. `Dispatcher::onPacketReleased(Packet*)`: clears optional bookkeeping before
   an outbound packet returns to the pool, including failures and invalid TX.
3. `Dispatcher::serviceOptionalPackets(uint32_t, const Packet* active)`: runs on
   every loop to release expired waiting copies. The active packet is excluded.
4. `Dispatcher::getOptionalPacket(uint32_t)`: borrows a ready, unexpired copy;
   selection transfers it to the dispatcher. It is selected only when no normal
   packet is due, and its record remains until completion/release.
5. `Dispatcher::deferOptionalPackets(uint32_t)`: applies the module's local retry
   pause on optional-only busy/budget waits, without updating normal timers.
6. `Dispatcher::allowPacketStart(Packet*, uint32_t)`: checks optional expiry and
   reception immediately before starting the radio. Rejected copies are freed.
7. `Dispatcher::onTxStarted(Packet*, uint32_t)`: called only after a successful
   radio start. Captures an original's context generation or marks a copy in
   flight. Every original/repeat release clears its transient bookkeeping.

All hooks default to inactive behaviour. Optional-only waits bypass shared
busy/backoff updates, while normal packets follow the existing dispatcher path.

`examples/simple_repeater/MyMesh.h` connects those hooks only when
`FLOOD_TEXT_DOUBLER` is defined. `variants/doubler/platformio.ini` defines it for
the custom builds and owns the release version. The policy holds packets already
allocated from the repeater's static pool; waiting copies need neither a new
allocation nor an outbound queue insertion.

`DoublerSettings.h` owns CLI parsing and schema validation. `DoublerFileStorage.h`
owns the temporary-file/readback/rename sequence. `MyMesh.cpp` loads the saved
mode at startup and intercepts this one CLI setting before the common fallback.
The opt-in profiles explicitly declare ArduinoJson for JSON validation; baseline
CLI and preference definitions are unchanged.

`DoublerContext.h` compares exact effective radio and packet-context values;
`FloodTextDoubler` owns the generation and cancellation state. Their fixed RAM
budgets are checked at compile time (80 bytes policy on nRF52, 64 bytes context).
Context observation, cancellation and optional scheduling perform no allocation.
On future baseline updates, audit every actual radio-apply, identity-import,
path-hash and default-scope entry point, including temporary radio restoration.

`CompanionFloodDoubler.h` connects the shared settings and local-only policy. The
companion `MyMesh` class uses the same dispatcher hooks, loads settings through
the primary filesystem, and handles only this CLI command before falling back
to its untouched baseline command parser. Its pending-work check includes
waiting/in-flight copies. There are no changes to the companion binary frame
protocol, `NodePrefs`, ACK handling or application notification generation.

When updating upstream, review the TX-completion/release hooks, pool ownership,
companion forwarding-record capacity, parent environment names and packet
classification, then rerun tests and all four builds. Update the version in the custom configuration, README and this document
together. Disabling the build flag removes the repeater policy; the hooks retain
default inactive behaviour. Stock firmware remains the rollback option.

## Automated validation

```sh
pio test -e native_doubler
pio test -e native -e native_kiss_modem
python3 variants/doubler/build.py
```

The doubler suite uses the production dispatcher, static pool, RNG mapping,
queue, serialization and duplicate table with a fake radio and clock. It covers
classification, both delay endpoints, fresh random draws, identical copies,
bounded repeats, priorities, reception, budget deferral, failure/timeout cleanup,
pool reuse, duplicate input, exact expiry boundaries, cleanup during other TX,
busy/budget starvation, normal-traffic ordering and CAD isolation, final-start
guards, and 32-bit delay/expiry rollover. Routing and
cryptographic interoperability still require hardware validation: native tests
use the repository's noncryptographic crypto mocks.

Mode/storage tests cover every mode/route/payload combination, direct ACK
behaviour, live transitions, default/query/strict validation, reloads, corrupt
and future files, and filesystem failures during open, write, close/readback,
temporary cleanup and rename. The real storage adapter runs against a
fault-injectable filesystem; this does not substitute for a physical power cut.

Lifecycle tests cover off/on and narrow/widen before cleanup, applied-radio
A/B/A transitions, context changes during original and optional TX, all radio
parameters, identity/hash/default-scope changes, availability transitions, final
start checks, distinct per-packet scopes, unchanged observations, save failures
with an existing waiting copy, pending-work cleanup and allocation-free policy
operations. A missing successful-start record fails closed.

Companion tests cover all mode/route/payload/origin combinations, one/two/three
byte path hashes, the 16-packet pool under burst/low-capacity conditions, normal
traffic priority, expiry, repeated application attempts, plain/prefixed CLI
replies, save/reload failures, future schemas and pending-work cancellation.
They exercise the shared adapter and real dispatcher with a fake radio. Live
USB/Bluetooth frames, app notification behaviour, disconnect/reconnect, remote
permissions, physical sleep and OTA transfers still require device validation.

## Hardware and field acceptance

Run the following for **each** modified board against an unmodified 1.18
repeater, and against the hill's actual version if different. Use packet logging
or a separate receiver to capture packets and timing. The UF2 builds alone do
not establish these results.

1. Save the previous stock image and node settings for rollback. On each board,
   exercise an OTA upgrade using its `.ota.zip` and compatible DFU updater.
   Confirm it reboots, reports the new version and has matching radio settings.
   Also verify UF2 flashing as the recovery route.
   Exercise `get multi.flood` and all three `set` values over serial and the
   authenticated remote CLI. Reboot after each change and verify persistence.
   Check invalid commands do not change the mode or stored value. Save a normal
   baseline preference, then perform a stock-to-doubler round trip with the
   filesystem preserved and confirm the module setting survives.
2. Send Public, hashtag and private channel text. Verify two identical outgoing
   frames; in an idle setup, the repeat starts 500–800 ms after first TX
   completion, allowing normal loop/radio scheduling overhead.
3. With both copies received, verify one normal processing/forwarding event at
   the stock repeater and a duplicate count increment for the other copy. Do not
   infer this from the chat UI alone.
4. Suppress the first copy using a controlled receiver/listening window or RF
   setup. Verify the second copy delivers the message without sender retry.
5. Repeat with region-scoped floods. Verify denied regions and disabled
   forwarding still prevent initial forwarding and therefore prevent doubling.
6. Check direct messages, adverts, ACKs and group data retain stock behaviour.
7. Send unrelated traffic during the gap, including a reception spanning the
   repeat deadline. Verify processing continues and channel checks defer TX.
8. Exercise bursts and bidirectional traffic. Check repeats remain bounded,
   ordinary traffic takes priority, TX accounting includes both copies, and the
   node does not reset or lose packet-pool capacity during sustained operation.
   Keep the channel busy past three seconds and verify the optional copy expires
   rather than transmitting later. Verify normal traffic retains its own CAD
   wait and is not delayed by a prior optional-only busy wait.
9. Alternate stock and doubler firmware over comparable hill-link test periods
   on the same board, antenna and settings. Record unique message delivery,
   latency, airtime, duplicates, failures and reverse-direction reception.

Accept the field change only if unique-message delivery improves without an
unacceptable reduction in reverse-direction reception or node stability. Keep
hardware observations separate from automated test/build results.

For each companion transport, additionally verify app connection and framed
CLI commands, one application notification per submitted message, saved modes
after reboot, and outgoing packet counts for scoped/unscoped channel text and
flood/direct messages. With companion forwarding enabled, verify that relayed
floods receive no additional copy. Disconnect/reconnect the app during a pending
repeat and verify that transmission/expiry continues and the node stays stable.
Verify zero-hop adverts remain single and no transmission occurs after the
three-second deadline. Exercise an OTA upgrade and UF2 recovery on both profiles.

For context acceptance, queue a copy, then change the applied radio profile
before its due time. Capture both profiles and verify no old optional frame is
sent on the new profile; a newly started original may repeat normally. Repeat
with an A/B/A switch and with identity/path-hash/default-scope changes. Verify
reboot-only radio edits and unrelated settings do not cancel a still-valid copy.
Keep hardware outcomes separate from the native/build checks above.
