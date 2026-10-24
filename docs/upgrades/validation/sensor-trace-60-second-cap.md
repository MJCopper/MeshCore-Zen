# Fixed 60-second trace cap

Date: 2026-10-25. Applies to the current working tree on `meshcore/sensor`,
based on `69891ef8` plus the existing recovery and acknowledgement changes.
This supersedes the calculated multi-minute timeout in the earlier
[acknowledgement record](sensor-trace-request-ack.md).

## Behaviour and implementation

Both `!hillvue trace` and `!hillvue trace 7188` acknowledge an accepted start with:

```text
BME680 Sensor: Trace requested, please wait up to 60 seconds.
```

The monotonic deadline is frozen at acceptance. `TraceTransactionBudget` now
provides a fixed 60,000 ms duration and a 5,000 ms reporting reserve rather than
summing maximum ten-hop phase allowances. The limit covers acknowledgement,
discovery, queue waits, settling, probes, retries, grace and final reporting.
Duplicate/start-busy/status requests and new routes cannot extend it.

At actual TX start, existing airtime/hop receive allowances are shortened to
share the remaining work time:

| Attempt | Maximum share of remaining time before the 55-second cutoff |
| --- | --- |
| Discovery 1 | One quarter |
| Discovery 2 | One third |
| Probe 1 | One half |
| Probe 2 | All remaining work time |

The first discovery leaves room for a discovery retry and two probes. A quick
successful discovery leaves more time for probing. Shorter existing phase
allowances remain shorter. Queue delays are included because the share is
calculated at actual TX start, using only timer arithmetic; observers do not
query the transmitting radio. A retry is not admitted if its estimated TX
watchdog plus requested delay cannot fit before the work cutoff.

On the first running poll at 55 seconds, unfinished work ends. A previously
stored valid fallback result can still be reported; otherwise the sensor queues
`Trace: discovery timed out` or `Trace: no result before timeout`, according to
the stage. A timeout generated at this cutoff has no application jitter. Its
report budget is limited to the original deadline. A report already active is
preserved and never replaced by a conflicting failure.

At 60 seconds, owned queued packets are cancelled and the transaction releases
its trace slot. Packets already on air remain exclusively under Dispatcher
ownership until completion/watchdog recovery. Late results, discovery replies
and callbacks do not reopen the expired transaction. Success can finish much
earlier, and the existing node order, averaged SNR and final result format are
preserved. Status includes the current phase and overall time remaining.

The cap bounds application work on a running service loop, not successful RF
receipt. Slow/congested links can time out despite being reachable. An in-flight
frame cannot be recalled, and a blocked application cannot clean up until its
next poll. Final multipart delivery is best effort within the same deadline;
remaining unsent parts cannot continue past 60 seconds.

## Validation

All software validation gates passed:

| Gate | Result |
| --- | --- |
| Native, sensor and KISS suites | 415/415 passed |
| Sensor AddressSanitizer suite | 308/308 passed |
| Five firmware environments: buildprog and UF2 | Passed |
| Five DFU archives: CRC and root application manifest/bin/dat | Passed |
| Sensor boundaries and whitespace checks | Passed |
| Strict MkDocs build | Passed; existing FAQ anchor INFO notice remains |

The final sensor uses 37,364 bytes of static RAM and 359,172 bytes of flash,
8 RAM and 32 flash bytes less than the preceding acknowledgement build.
The environments were `Xiao_nrf52_bme680_sensor`, `Xiao_nrf52_sensor_baseline`,
`Xiao_nrf52_companion_radio_usb`, `Xiao_nrf52_repeater`, and
`Xiao_nrf52_room_server`.

[Check summary and SHA256 values](sensor-trace-60-second-cap-checks.txt) record
all generated HEX/DFU/UF2 artifacts. The sensor OTA package is
`.pio/build/Xiao_nrf52_bme680_sensor/MeshCore_BME680_Sensor_Trace_60s_OTA_2026-10-25.zip`.
It is a direct DFU archive with its application manifest at the root, copied
byte-for-byte from the verified build's `firmware.zip`. SHA256:
`a074c5cf5c6cd6f57221abc65fa83682211abb07e9cb493598abf09e7c37f4e5`.

Focused coverage includes:

- Both commands acknowledge exactly 60 seconds, independent of route length,
  one/two-byte hashes, radio airtime and scoped/unscoped reply context.
- Work and transaction boundaries at 55 and 60 seconds, adjacent milliseconds,
  rollover and delayed polls.
- Discovery/probe receive-window clamping at TX start; shorter ordinary windows
  remain intact; slow-radio tracing can still use the second probe attempt.
- A stage-specific discovery or probe timeout uses the final reporting window;
  a queued failure cannot acquire a fresh window after expiry.
- Late probe replies during the report reserve cannot replace a timeout or
  restart work; late completion preserves Dispatcher ownership.
- Cancellation, exhausted queues, allocation/queue/start failures, lost callbacks,
  status, duplicates, new trace after cleanup, and existing success/fallback/SNR
  and scope/history regressions.

The older exact-phase tests continue to isolate acknowledgement through their
explicit transport seam. Dedicated acknowledgement tests and the network model
use the real Dispatcher and encryption. Native network tests model radio links
and contention; they are not physical RF acceptance.

Commands:

```sh
pio test -e native -e native_sensor -e native_kiss_modem
ASAN_OPTIONS=detect_leaks=0 pio test -c /tmp/meshcore-scope-asan.ini -e native_sensor_asan
pio run -e Xiao_nrf52_bme680_sensor -e Xiao_nrf52_sensor_baseline \
  -e Xiao_nrf52_companion_radio_usb -e Xiao_nrf52_repeater \
  -e Xiao_nrf52_room_server -t buildprog -t create_uf2
bash bin/check-sensor-boundaries.sh
git diff --check
mkdocs build --strict
```

PlatformIO on this host is `/home/matt/.platformio/penv/bin/pio`. ASan uses the
existing isolated build/configuration and extracted runtime documented in the
previous acknowledgement validation. Leak detection is disabled; no sanitizer
runtime is included in firmware.

## Outstanding physical investigation

No device has been flashed and no hardware acceptance has been performed. Check
a successful and a silent target, bare and targeted commands, busy/slow radio,
cancellation, 55/60-second cleanup and a subsequent request without reboot.

This change does **not** fix or establish the cause of the sensor discovery
failure for `7188`. Zen succeeding from the same location does not establish
sensor identity/ACL eligibility, scope equivalence or acceptance of returning
replies. That read-only comparison and packet-level diagnosis remain a separate
investigation. The 60-second limit shortens waiting and guarantees bounded
software ownership; it does not claim that the destination is unreachable.
