# Optional repeated flood transmission

Sensor release **v1.18.0-sensor.1** adds `multi.flood`, ported from
`meshcore/doubler` commit `7f0cb629`. It applies to packets originating on the
sensor, including replies generated in response to received commands. It keeps
the sensor's existing forwarding-disabled behaviour.

## Configure

Use the existing local serial CLI or authenticated remote administrator CLI:

```text
get multi.flood
> 0
set multi.flood 1
OK
get multi.flood.stats
```

| Value | Eligible additional transmission |
|---|---|
| `0` | Disabled; default when no settings file exists |
| `1` | Locally originated flood channel text: Public, hashtag and private channels |
| `2` | All locally originated flood payload types: text, channel data, requests, guest login, ACKs, adverts and responses |

Direct packets remain unchanged in every mode. Direct round-trip TRACE probes
are not repeated. A local advert sent using `sendZeroHop()` has direct-route
encoding and is also excluded. A flooded advert qualifies in mode 2.
Both ordinary and region-scoped floods qualify. Received/forwarded packets are
excluded, including unusual zero-path forwarded packets tracked explicitly by
the adapter. No public bot command can change the mode. `get repeat` continues to report
`off`: it controls forwarding, independently of locally originated flood copies.

Only complete values `0`, `1`, `2` are accepted. Queries and unchanged values do
not write storage. CLI prefixes continue through the existing sensor handler.

## Transmission behaviour

After successful local completion of an eligible original, retain its packet
for one best-effort additional transmission. The ready delay is random
**500–800 ms inclusive** and expiry is **3,000 ms after original completion**.
The additional frame preserves header, payload, timestamps, path and transport
codes byte for byte. It does not pass through flood routing again.

Normal due traffic always has priority. Optional-only busy or airtime-budget
waits use a separate 200 ms defer period, preserving the normal channel/backoff
state. Optional copies never force TX through a busy-channel timeout. A final
check rejects expired, newly busy or cancelled copies during TX preparation.
Failed or timed-out originals are not repeated. A failed optional copy is
released without another attempt. No packet gets a third transmission.

Both physical transmissions are logged and counted in normal radio statistics,
airtime and duty accounting. The additional TX creates another interval during
which the sensor cannot receive. It can improve delivery after a lost original,
but is not a delivery guarantee or a replacement for normal application ACKs.

At most four optional packets may wait, and the original is retained only when
at least four pool slots remain free. The sensor adapter tracks the 32-packet
pool. Pending copies live outside the normal outbound queue, so its queue-length
statistic does not include those held packets. They expire under sustained normal
traffic, low airtime budget or persistent reception. An active transmission is
never freed by optional cleanup; Dispatcher retains ownership through completion
or watchdog recovery.

## Settings and context

Settings use a separate `/doubler.json`:

```json
{"schema":1,"multi_flood":1}
```

A missing file defaults off without a boot write. A corrupt file fails off and
can be repaired with an explicit valid set command. Unknown schemas/fields,
oversized/unreadable files fail off and reject edits, preserving the file.
Writes use `/doubler.tmp`, close/read-back verification and atomic rename. Failure
preserves the previous runtime mode and file. A leftover temporary file is
ignored at boot. Filesystem-full/save failures are reported; the firmware does
not enlarge or reformat the filesystem. Physical power-loss testing remains
separate from the fault-injected native tests.

No NodePrefs or baseline preference format changes are introduced. Mode persists
across reboot when the filesystem survives. Flashing does not reset radio
preferences. A saved `/doubler.json` from another role may retain its chosen
mode when switching firmware on the same device.

Disabling mode cancels waiting copies on the next scheduler pass. Mode changes
latch ineligibility immediately: off/on cannot revive an already cancelled
copy. Moving from mode 2 to 1 similarly cancels waiting non-text copies.
Applied frequency/BW/SF/CR changes, active identity changes, path-hash mode and
default scope changes invalidate old copies. Context A → B → A cannot revive
an old packet. Temporary radio application and restoration are observed at the
actual application points. Reboot-only preference edits do not change the
applied-radio snapshot. Per-message scope overrides remain on each packet.

## Trace integration

Physical optional transmissions are separate from logical trace events:

- Repeated acknowledgements do not start another trace or reset its timers.
- Three logical guest-login attempts may produce up to six physical login frames;
  each logical attempt still has its own durable timestamp. Its optional copy
  preserves that same timestamp and ciphertext.
- Discovery/login receive windows begin at original TX completion. Optional
  completion cannot restart or extend them.
- A copied report part cannot advance multipart delivery again. Its failure
  cannot turn a delivered original report into a logical failure.
- Trace-owned packets carry an internal operation identifier and frozen deadline;
  these are not wire fields. Closing/cancelling trace work invalidates its
  existing retained copies. Newly created terminal reports are tagged separately
  and may have their permitted copy after logical completion, within the deadline.
- A new trace cannot mistake an older report copy for its own packet.
- Copy admission rechecks the operation deadline after radio preparation and
  requires enough time for its TX watchdog. Waiting copies stop at cancellation
  or expiry; already transmitting packets finish under Dispatcher ownership.

The acknowledgement's calculated maximum and 120-second safety ceiling remain
unchanged. Additional traffic uses normal airtime and existing queue budgets;
optional copies are skipped rather than extending the promise. The existing
logical limits are at most two discovery requests, three login attempts and
two direct TRACE probes. Including one acknowledgement and two report parts,
the maximum original source count is ten. Mode 1 can add up to three text copies
(13 physical transmissions total); mode 2 can add up to eight flood copies
(18 total, including the two direct probes). Actual counts are often lower.

Stock receivers normally suppress identical copies through their duplicate
cache. Restart/cache eviction can allow a second delivery; the local three-second
expiry is not an exactly-once guarantee for remote devices.

## Diagnostics and verification

`get multi.flood.stats` returns boot-local counts of retained, physically started,
expired, cancelled, skipped and failed optional copies plus pending count.
The query performs no storage writes or radio sends. A failed optional start
counts as failed without increasing logical trace failures.

The source branch's policy/setting regression tests are retained. Sensor tests
exercise the production adapter with real Dispatcher callbacks, pool ownership,
trace cancellation, interleaved operations and a variable-packet-airtime Australia
Narrow SF7/BW62.5/CR7 network. The normal sensor/baseline/companion/repeater/room
server targets are build regressions. Zen firmware is outside this change.
See the [validation record](../upgrades/validation/sensor-multi-flood.md) for
results, artifacts and pending physical acceptance.
