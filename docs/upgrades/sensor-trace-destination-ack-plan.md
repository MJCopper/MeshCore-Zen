# Named destination in trace acknowledgement

Status: software implemented and tested; physical acceptance pending. Date: 2026-10-25.

See the [validation record](validation/sensor-trace-destination-ack.md).

## Required behaviour

For a uniquely known target with prefix `7188` and name `Hilltop Repeater`:

```text
Command: !hillvue trace 7188
Body: Trace requested to Hilltop Repeater, please wait up to 60 seconds.
On air: BME680 Sensor: Trace requested to Hilltop Repeater, please wait up to 60 seconds.
```

For a known full identity with no usable name:

```text
Trace requested to 7188, please wait up to 60 seconds.
```

Preserve bare trace exactly:

```text
Trace requested, please wait up to 60 seconds.
```

Preserve current errors, including the normal sensor-name prefix:

```text
BME680 Sensor: Trace: unknown repeater 7188
BME680 Sensor: Trace: ambiguous repeater 7188
```

An unknown or ambiguous target starts neither acknowledgement nor discovery.
The fallback label applies only to an already resolved full identity; four hex
characters alone cannot supply the authenticated discovery public key.

The existing 60-second transaction deadline and 55-second work cutoff remain
in force. Displaying a name does not alter discovery, permissions, scope, retry
counts, trace routing, averaged SNR, final result formatting or command-rate rules.

## Current implementation and design decision

`RepeaterNameCache` stores up to 48 entries containing a full public key and a
32-byte name buffer. `resolvePrefix` currently returns the unique full key but
no name. `formatLabel` is intended for compact hop labels: it resolves short
hashes, removes words such as `Repeater`, and truncates to its destination buffer.
It must not be reused for this acknowledgement, since that would change
`Hilltop Repeater` to `Hilltop`.

`TraceRequestAcknowledgement` already copies the entire formatted message,
including sensor-name prefix, into a bounded buffer before admission. Keep that
ownership model: no pointer into the mutable cache survives acceptance.

## Implementation sequence

1. Extend the cache lookup with a compatible overload returning the unique full
   key and a bounded destination display label together. Preserve existing
   callers and compact path/hop formatting. Resolve both outputs from the same
   entry in the same lookup; do not look the name up again by a shorter hash.
2. Clear output key/label on unknown or ambiguous resolution so neither can be
   accidentally used as a successful lookup. Prefix comparison remains exactly
   two bytes and uniqueness is based on full identities, not names.
3. Build the destination display label from the stored full name. Preserve
   meaningful words, punctuation, case and valid UTF-8, including `Repeater`.
   Normalize control characters to spaces, trim leading/trailing whitespace and
   collapse repeated whitespace for a single-line acknowledgement. Respect the
   existing stored name limit. Do not change the cache file format or stored
   names. A name consisting only of whitespace/unsafe or unusable text falls
   back to the four-character lowercase hex prefix.
4. In the targeted command branch, complete existing identity/path-width
   validation before accepting. Pass the local label snapshot into
   `beginAcknowledgement`; keep `discovery.begin` supplied with the full resolved
   public key. Do not derive protocol identity from a name or fallback label.
5. Extend acknowledgement formatting with an optional destination argument.
   Named starts use `Trace requested to Y, please wait up to 60 seconds.`;
   absence of that argument preserves the exact bare message. Copy the final
   text immediately, including destination and sensor prefixes.
6. Retain zero added application jitter, separate acknowledgement ownership,
   bounded definitely-unsent retries, and no duplicate after possible emission.
   Cache rename/eviction, new adverts and sensor rename during queue retry must
   not change accepted text, identity, reply context or deadline.
7. Update public bot/developer documentation with named and fallback examples,
   and record software validation separately from physical acceptance.

## Message length and encoding policy

The complete text, including `sensor-name: `, must fit the existing single-part
public response limit (160 bytes). Use byte counts and bounded formatting;
the terminator requires its own byte. Do not split the acknowledgement.

Preserve the entire fixed message and timeout suffix. If space is constrained,
shorten only the destination label at a valid UTF-8 boundary. Prefer the full
stored name whenever it fits. If no meaningful destination text fits, try the
four-character key-prefix fallback. If even that cannot fit, fail before
acceptance through the existing bounded formatting-error path; do not silently
omit the destination, produce an empty label or send a truncated timeout.

Normal 31-byte cached names should fit with the normal sensor prefix. Test
extreme sensor-prefix lengths explicitly rather than assuming this arithmetic.
An entirely unknown nameless advert must retain current cache admission
behaviour: the cache currently ignores adverts without a name field. Do not
silently expand cache admission merely to exercise the unnamed fallback.

## Files expected to change

| File/area | Purpose |
| --- | --- |
| `RepeaterNameCache.h/.cpp` | Full-identity lookup with full destination label and fallback |
| `TraceRequestAcknowledgement.h` | Optional named destination and bounded one-part formatting |
| `SensorBotService.h/.cpp` | Pass the acceptance snapshot through the targeted start path |
| `test_sensor_storage` | Cache resolution/name/encoding tests |
| `test_sensor_trace_ack` | Exact text, message length, immutability and acknowledgement ownership |
| `test_sensor_target_trace` | Full-key authenticated discovery and unchanged errors |
| `test_sensor_network` where assertions need extension | Named acknowledgement over real simulated links |
| Public bot/development docs and validation record | User behaviour and evidence |

No changes are expected in core Mesh/Dispatcher, repeater ACL/authentication,
cache persistence schema, parser syntax or the 60-second timing policy.

## Test matrix

| Scenario | Required assertion |
| --- | --- |
| Unique `7188`, `Hilltop Repeater` | Exact named body and ordinary sensor-name prefix |
| Name contains `Repeater` or `RPT` | Preserve those words in destination acknowledgement; old hop formatting stays unchanged |
| Uppercase hex input | Same full identity/name; canonical lowercase prefix on fallback |
| Two full keys share `7188` | Exact ambiguous error; no acknowledgement, discovery or selected identity |
| Two devices share a name but not key prefix | Resolve the requested full key; names do not determine identity |
| Unknown `7188` | Exact unknown error; no fallback discovery |
| Known identity, empty/whitespace name | Use `7188`; discovery still uses all 32 key bytes |
| Advert without a name field | Existing cache admission/unknown behaviour is preserved |
| Stored name with controls/repeated whitespace | Single-line normalized label; no injected response line |
| UTF-8 and emoji names | Preserve valid characters; no split code point at storage/display/message boundaries |
| Invalid/incomplete UTF-8 | Bounded usable label or hex fallback; never malformed/truncated output |
| Maximum stored name | Full name shown when it fits, rather than the shorter hop-label limit |
| Exact 160-byte message and one-byte overflow | Exact-fit succeeds; overflow shortens only label on valid boundary |
| Very long sensor-name prefix | Prefix fallback if it fits; otherwise bounded pre-acceptance failure |
| Cache or sensor renamed during allocation/queue retries | Byte-identical acknowledgement snapshot and unchanged full target key |
| Cache entry evicted after acceptance | Snapshot remains valid; no pointer into replaced cache memory |
| Definitely-unsent rejection and retry | Same named text; at most one emitting acknowledgement |
| Uncertain start/completion, lost callback | Existing no-repeat/recovery behaviour preserved |
| Duplicate, busy, rate-limited, status or cancel | No new named acknowledgement; no deadline reset |
| Bare trace | Exact existing acknowledgement, routing and final output |
| Both channels, hash widths and scopes | Frozen destination text and correct reply context |
| 55/60-second boundaries and rollover | Existing report reserve, expiry and busy release remain intact |

For identity-sensitive tests, decrypt the actual outgoing discovery request with
the selected full target's shared secret and verify the RTC replay tag. Include
another identity with a different remaining key suffix so a name-only or partial
key implementation cannot pass. Unknown/ambiguous tests should observe packet
types/counts and pool ownership, not merely the returned label.

For immutability, force a queue rejection or allocation retry, then rename or
evict the cache entry before transmission. Inspect emitted text and the actual
discovery identity. Retain the existing real Dispatcher tests; do not rely only
on the legacy immediate-acknowledgement seam.

## Validation gates

1. Run focused storage, acknowledgement, targeted trace and network tests.
2. Run `pio test -e native -e native_sensor -e native_kiss_modem`.
3. Run the sensor AddressSanitizer suite using the existing working host
   configuration; record its leak-check setting/runtime setup.
4. Build `Xiao_nrf52_bme680_sensor`, `Xiao_nrf52_sensor_baseline`,
   `Xiao_nrf52_companion_radio_usb`, `Xiao_nrf52_repeater`, and
   `Xiao_nrf52_room_server`; generate applicable DFU and UF2 artifacts.
5. Run `bin/check-sensor-boundaries.sh`, whitespace checks and strict MkDocs.
6. Compare static RAM/flash with the pre-change build. Keep bounded stack/static
   storage and avoid a per-request heap allocation or new persistence writes.
7. Record exact results, revision/working-tree status and artifact checksums.
   Produce a uniquely named sensor OTA if packaging is requested; do not flash
   or publish automatically.

Physical acceptance, when hardware is available: advertise a known named
repeater, request it by prefix, verify the exact named acknowledgement and usual
trace result, then repeat after a rename and with an unusable name. Confirm a
pending request retains its original label while a later accepted request uses
the updated name. Check unknown/ambiguous errors, both channels and the unchanged
60-second ceiling. Mark physical evidence pending until actually performed.

## Completion criteria

Every accepted targeted trace names the same full identity used for discovery,
using a frozen full-name label or the defined prefix fallback. All rejection
paths and bare trace behaviour remain unchanged. Messages remain single-part,
UTF-8 safe and bounded. Ownership, retry, scope and timing regressions pass, with
hardware results explicitly distinguished from software tests.

This change concerns acknowledgement text. It does not resolve or diagnose the
existing sensor-versus-Zen discovery failure.
